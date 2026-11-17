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
        rolling_restart_ = false;
        snap_.attempt = 0;
        ever_connected_ = false;
        reset_calls();
        return start_engine();
    case ConnectionState::Starting:
    case ConnectionState::Connecting:
    case ConnectionState::Connected:
    case ConnectionState::Captcha:
        if (profile_ && *profile_ == p) {
            return ConnectResult::AlreadyActive;
        }
        pending_ = p;
        rolling_restart_ = false;
        transition(ConnectionState::Stopping);
        notify();
        engine_.stop();
        return ConnectResult::Switching;
    case ConnectionState::Stopping:
        pending_ = p;
        rolling_restart_ = false;
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
    const bool roll = rolling();
    const int groups =
        p.provider == kProviderVk && !roll ? std::max<int>(static_cast<int>(p.secrets.links.size()), 1) : 1;
    snap_.total = p.effective_streams() * groups;
    snap_.error.clear();

    EngineLaunch launch;
    launch.profile = p;
    if (roll) {
        if (snap_.calls.size() != p.secrets.links.size()) {
            reset_calls();
        }
        launch.profile.secrets.links = {p.secrets.links[call_]};
        snap_.calls[call_].state = CallState::Active;
        log(LogLevel::Info, std::format("rolling calls: call {} of {}", call_ + 1, snap_.calls.size()));
    }
    notify();
    launch.args = build_core_args(launch.profile);
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
    if (roll) {
        timer_.schedule(kCallTimeoutMs);
    }
    notify();
    return ConnectResult::Started;
}

void ConnectionManager::disconnect() {
    pending_.reset();
    rolling_restart_ = false;
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
    // Пока хоть один поток жив, звонок работает: ошибка ссылки - только на новых потоках.
    if (rolling() && st.link_error && st.active == 0) {
        log(LogLevel::Warn, std::format("rolling calls: call {} rejected by VK", call_ + 1));
        roll_call();
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
        if (rolling()) {
            if (target == ConnectionState::Connecting) {
                timer_.schedule(kCallTimeoutMs);
            } else if (s == ConnectionState::Connecting) {
                timer_.cancel();
            }
        }
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
            if (!rolling_restart_) {
                reset_calls();
            }
            rolling_restart_ = false;
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

    if (rolling() && st.link_error) {
        if (advance_call()) {
            snap_.attempt = 0;
            transition(ConnectionState::Reconnecting);
            timer_.schedule(reconnect_delay_ms(1));
            log(LogLevel::Warn, std::format("rolling calls: switching to call {}", call_ + 1));
        } else {
            snap_.error = "all VK calls failed";
            transition(ConnectionState::Error);
        }
        notify();
        return;
    }

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
    // При роллинге тот же таймер в Connecting - срок звонка.
    if (rolling() && snap_.state == ConnectionState::Connecting) {
        log(LogLevel::Warn,
            std::format("rolling calls: call {} gave no streams in {} s", call_ + 1, kCallTimeoutMs / 1000));
        roll_call();
        return;
    }
    if (snap_.state != ConnectionState::Reconnecting) {
        return;
    }
    start_engine();
}

bool ConnectionManager::rolling() const {
    return profile_ && profile_->rolling_calls && profile_->provider == kProviderVk &&
           profile_->secrets.links.size() > 1;
}

void ConnectionManager::reset_calls() {
    call_ = 0;
    snap_.calls.clear();
    if (rolling()) {
        for (const auto& l : profile_->secrets.links) {
            snap_.calls.push_back({l, CallState::Unchecked});
        }
    }
}

// Текущий звонок - в красные, дальше по кругу первый не красный. false - красные все.
bool ConnectionManager::advance_call() {
    snap_.calls[call_].state = CallState::Failed;
    const std::size_t n = snap_.calls.size();
    for (std::size_t k = 1; k < n; ++k) {
        const std::size_t j = (call_ + k) % n;
        if (snap_.calls[j].state != CallState::Failed) {
            call_ = j;
            return true;
        }
    }
    return false;
}

void ConnectionManager::roll_call() {
    timer_.cancel();
    if (advance_call()) {
        log(LogLevel::Warn, std::format("rolling calls: switching to call {}", call_ + 1));
        pending_ = profile_;
        rolling_restart_ = true;
    } else {
        log(LogLevel::Error, "rolling calls: all VK calls failed");
        snap_.error = "all VK calls failed";
        pending_.reset();
    }
    transition(ConnectionState::Stopping);
    notify();
    engine_.stop();
}

} // namespace ft
