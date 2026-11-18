#pragma once

#include <string_view>

namespace ft {

enum class ConnectionState {
    Disconnected,
    Starting,
    Connecting,
    Connected,
    Captcha,
    Reconnecting,
    Stopping,
    Error,
};

std::string_view to_string(ConnectionState s);

// Таблица разрешённых переходов: всё, чего в ней нет, - баг в ConnectionManager.
bool can_transition(ConnectionState from, ConnectionState to);

// Ядро запущено или вот-вот будет - профиль и настройки подключения менять нельзя.
bool is_busy(ConnectionState s);

} // namespace ft
