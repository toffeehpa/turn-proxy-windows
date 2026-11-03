#include "core/connection_manager.h"

#include <algorithm>
#include <format>

#include "core/core_args.h"

namespace ft {

std::uint32_t reconnect_delay_ms(int attempt) {
    if (attempt <= 0) {
        return 0;
    }
    if (attempt >= 5) {
        return 30'000;
    }
    return 1000u << attempt;
}

ConnectionManager::ConnectionManager(IEngine& engine, IReconnectTimer& timer)
    : engine_(engine), timer_(timer) {}

void ConnectionManager::log(LogLevel level, std::string_view text) {
    if (on_log_) {
        on_log_(level, text);
    }
}

void ConnectionManager::notify() {
    if (on_change_) {
        on_change_(snap_);
    }
}

bool ConnectionManager::transition(ConnectionState to) {
    if (!can_transition(snap_.state, to)) {
        log(LogLevel::Error,
            std::format("internal: transition {} -> {} rejected", to_string(snap_.state), to_string(to)));
        return false;
    }
    log(LogLevel::Debug, std::format("state {} -> {}", to_string(snap_.state), to_string(to)));
    snap_.state = to;
    return true;
}

ConnectResult ConnectionManager::connect(const Profile& p) {
    if (const auto issues = validate(p); !issues.empty()) {
        log(LogLevel::Warn, std::format("profile '{}' is invalid: {}", p.name, to_string(issues.front())));
        return ConnectResult::InvalidProfile;
    }
    if (p.provider == kProviderSfu && !opts_.allow_sfu) {
        return ConnectResult::ExperimentalDisabled;
    }

    switch (snap_.state) {
    case ConnectionState::Disconnected:
    case ConnectionState::Error:
    case ConnectionState::Reconnecting:
        timer_.cancel();
        profile_ = p;
        pending_.reset();
        snap_.attempt = 0;
        ever_connected_ = false;
        return start_engine();
    case ConnectionState::Starting:
    case ConnectionState::Connecting:
    case ConnectionState::Connected:
    case ConnectionState::Captcha:
        if (profile_ && *profile_ == p) {
            return ConnectResult::AlreadyActive;
        }
        pending_ = p;
        transition(ConnectionState::Stopping);
        notify();
        engine_.stop();
        return ConnectResult::Switching;
    case ConnectionState::Stopping:
        pending_ = p;
        return ConnectResult::Queued;
    }
    return ConnectResult::LaunchFailed;
}

ConnectResult ConnectionManager::start_engine() {
    if (!profile_ || !transition(ConnectionState::Starting)) {
        return ConnectResult::LaunchFailed;
    }
    const Profile& p = *profile_;
    ++session_;
    parser_.reset();
    redactor_ = {};
    redactor_.add(p.secrets.obf_key);
    redactor_.add(p.secrets.client_id);
    for (const auto& l : p.secrets.links) {
        redactor_.add(l);
    }

    snap_.profile_id = p.id;
    snap_.profile_name = p.name;
    snap_.local_address = p.local_address();
    snap_.active = 0;
    const int groups =
        p.provider == kProviderVk ? std::max<int>(static_cast<int>(p.secrets.links.size()), 1) : 1;
    snap_.total = p.effective_streams() * groups;
    snap_.error.clear();
    notify();

    EngineLaunch launch;
    launch.profile = p;
    launch.args = build_core_args(p);
    launch.display_args = redact_core_args(launch.args);
    std::string shown;
    for (const auto& a : launch.display_args) {
        shown += ' ';
        shown += a;
    }
    log(LogLevel::Info, std::format("connect profile='{}' attempt={} args:{}", p.name, snap_.attempt, shown));

    auto started = engine_.start(session_, launch);
    if (!started) {
        snap_.error = started.error();
        log(LogLevel::Error, snap_.error);
        transition(ConnectionState::Error);
        notify();
        return ConnectResult::LaunchFailed;
    }
    transition(ConnectionState::Connecting);
    notify();
    return ConnectResult::Started;
}

void ConnectionManager::disconnect() {
    pending_.reset();
    switch (snap_.state) {
    case ConnectionState::Starting:
    case ConnectionState::Connecting:
    case ConnectionState::Connected:
    case ConnectionState::Captcha:
        transition(ConnectionState::Stopping);
        notify();
        engine_.stop();
        break;
    case ConnectionState::Reconnecting:
        timer_.cancel();
        transition(ConnectionState::Disconnected);
        snap_.active = 0;
        notify();
        break;
    case ConnectionState::Error:
        transition(ConnectionState::Disconnected);
        snap_.error.clear();
        notify();
        break;
    case ConnectionState::Disconnected:
    case ConnectionState::Stopping:
        break;
    }
}

void ConnectionManager::reconnect() {
    if (!profile_) {
        return;
    }
    switch (snap_.state) {
    case ConnectionState::Starting:
    case ConnectionState::Connecting:
    case ConnectionState::Connected:
    case ConnectionState::Captcha:
        pending_ = profile_;
        transition(ConnectionState::Stopping);
        notify();
        engine_.stop();
        break;
    case ConnectionState::Disconnected:
    case ConnectionState::Error:
    case ConnectionState::Reconnecting: {
        const Profile p = *profile_;
        connect(p);
        break;
    }
    case ConnectionState::Stopping:
        pending_ = profile_;
        break;
    }
}

void ConnectionManager::on_engine_event(const EngineEvent& ev) {
    // События от прошлого запуска приходят после Exited - игнорируем.
    if (ev.session != session_) {
        return;
    }
    if (ev.kind == EngineEvent::Kind::Exited) {
        handle_exit(ev.exit_code);
        return;
    }
    const auto line = split_core_log_line(ev.line);
    if (line.level != LogLevel::Debug || opts_.verbose_core_log) {
        const LogLevel level = line.level == LogLevel::None ? LogLevel::Info : line.level;
        log(level, "core: " + redactor_.apply(line.text));
    }
    if (parser_.feed(ev.line)) {
        apply_status();
    }
}

void ConnectionManager::apply_status() {
    const auto& st = parser_.status();
    snap_.active = st.active;
    if (st.total > 0) {
        snap_.total = st.total;
    }
    const auto s = snap_.state;
    if (s != ConnectionState::Connecting && s != ConnectionState::Connected &&
        s != ConnectionState::Captcha) {
        notify();
        return;
    }
    ConnectionState target = ConnectionState::Connecting;
    if (st.captcha) {
        target = ConnectionState::Captcha;
    } else if (st.active > 0) {
        target = ConnectionState::Connected;
        ever_connected_ = true;
        snap_.attempt = 0;
        snap_.error.clear();
    }
    if (target != s) {
        transition(target);
    }
    notify();
}

void ConnectionManager::handle_exit(int exit_code) {
    log(LogLevel::Info, std::format("freeturn-core.exe exited, code={}", exit_code));
    snap_.active = 0;

    if (snap_.state == ConnectionState::Stopping) {
        transition(ConnectionState::Disconnected);
        if (pending_) {
            profile_ = std::move(pending_);
            pending_.reset();
            snap_.attempt = 0;
            ever_connected_ = false;
            start_engine();
            return;
        }
        notify();
        return;
    }

    const auto s = snap_.state;
    if (s != ConnectionState::Starting && s != ConnectionState::Connecting &&
        s != ConnectionState::Connected && s != ConnectionState::Captcha) {
        return;
    }

    const auto& st = parser_.status();
    snap_.error = st.last_error.empty() ? std::format("freeturn-core.exe exited with code {}", exit_code)
                                        : redactor_.apply(st.last_error);

    // Ошибки конфига и проваленная капча повторами не лечатся - ждём пользователя.
    if (opts_.auto_reconnect && ever_connected_ && !st.fatal && snap_.attempt < kMaxReconnectAttempts) {
        ++snap_.attempt;
        transition(ConnectionState::Reconnecting);
        timer_.schedule(reconnect_delay_ms(snap_.attempt));
        log(LogLevel::Warn,
            std::format("reconnect attempt {} in {} ms", snap_.attempt, reconnect_delay_ms(snap_.attempt)));
    } else {
        transition(ConnectionState::Error);
    }
    notify();
}

void ConnectionManager::on_reconnect_timer() {
    if (snap_.state != ConnectionState::Reconnecting) {
        return;
    }
    start_engine();
}

} // namespace ft
