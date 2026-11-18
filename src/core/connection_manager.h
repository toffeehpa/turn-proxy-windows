#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/connection_state.h"
#include "core/engine.h"
#include "core/log_parser.h"
#include "core/profile.h"

namespace ft {

enum class CallState { Unchecked, Active, Failed };

struct CallStatus {
    std::string link;
    CallState state = CallState::Unchecked;

    bool operator==(const CallStatus&) const = default;
};

struct ConnectionSnapshot {
    ConnectionState state = ConnectionState::Disconnected;
    std::string profile_id;
    std::string profile_name;
    std::string local_address;
    int active = 0;
    int total = 0;
    int attempt = 0;
    std::string error;
    // Только при роллинге звонков, в порядке очереди профиля.
    std::vector<CallStatus> calls;
};

struct ConnectionOptions {
    bool auto_reconnect = true;
    bool allow_sfu = false;
    // [DEBUG]-строки ядра в лог: шумно, по умолчанию выкл.
    bool verbose_core_log = false;
};

enum class ConnectResult {
    Started,
    AlreadyActive,
    Switching,
    Queued,
    InvalidProfile,
    ExperimentalDisabled,
    LaunchFailed
};

inline constexpr int kMaxReconnectAttempts = 5;
// Роллинг: столько ждём хотя бы один поток от звонка, пока не перейти к следующему. Капча не в счёт.
inline constexpr std::uint32_t kCallTimeoutMs = 60'000;

// Пауза перед попыткой n (с 1): 2, 4, 8, 16, 30 с.
std::uint32_t reconnect_delay_ms(int attempt);

// Единственный владелец состояния подключения. Живёт на UI-потоке, все методы вызываются оттуда.
class ConnectionManager {
public:
    using ChangeHandler = std::function<void(const ConnectionSnapshot&)>;
    using LogHandler = std::function<void(LogLevel, std::string_view)>;

    ConnectionManager(IEngine& engine, IReconnectTimer& timer);

    void set_options(const ConnectionOptions& opts) { opts_ = opts; }
    void on_change(ChangeHandler h) { on_change_ = std::move(h); }
    void on_log(LogHandler h) { on_log_ = std::move(h); }

    ConnectResult connect(const Profile& p);
    void disconnect();
    void reconnect();

    void on_engine_event(const EngineEvent& ev);
    void on_reconnect_timer();

    const ConnectionSnapshot& snapshot() const { return snap_; }
    ConnectionState state() const { return snap_.state; }
    const std::optional<Profile>& current_profile() const { return profile_; }

private:
    bool transition(ConnectionState to);
    ConnectResult start_engine();
    void apply_status();
    void handle_exit(int exit_code);
    bool rolling() const;
    void reset_calls();
    bool advance_call();
    void roll_call();
    void log(LogLevel level, std::string_view text);
    void notify();

    IEngine& engine_;
    IReconnectTimer& timer_;
    ConnectionOptions opts_;
    ChangeHandler on_change_;
    LogHandler on_log_;

    ConnectionSnapshot snap_;
    std::optional<Profile> profile_;
    // Куда переключиться, когда текущее ядро остановится.
    std::optional<Profile> pending_;
    SessionId session_ = 0;
    CoreLogParser parser_;
    SecretRedactor redactor_;
    bool ever_connected_ = false;
    std::size_t call_ = 0;
    // Перезапуск ядра ради следующего звонка: статусы звонков не сбрасываем.
    bool rolling_restart_ = false;
};

} // namespace ft
