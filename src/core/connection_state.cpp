#include "core/connection_state.h"

namespace ft {

std::string_view to_string(ConnectionState s) {
    switch (s) {
    case ConnectionState::Disconnected:
        return "disconnected";
    case ConnectionState::Starting:
        return "starting";
    case ConnectionState::Connecting:
        return "connecting";
    case ConnectionState::Connected:
        return "connected";
    case ConnectionState::Captcha:
        return "captcha";
    case ConnectionState::Reconnecting:
        return "reconnecting";
    case ConnectionState::Stopping:
        return "stopping";
    case ConnectionState::Error:
        return "error";
    }
    return "unknown";
}

bool can_transition(ConnectionState from, ConnectionState to) {
    using S = ConnectionState;
    if (from == to) {
        return false;
    }
    switch (from) {
    case S::Disconnected:
        return to == S::Starting;
    case S::Starting:
        return to == S::Connecting || to == S::Stopping || to == S::Error;
    case S::Connecting:
        return to == S::Connected || to == S::Captcha || to == S::Stopping || to == S::Error ||
               to == S::Reconnecting;
    case S::Connected:
        return to == S::Connecting || to == S::Captcha || to == S::Stopping || to == S::Error ||
               to == S::Reconnecting;
    case S::Captcha:
        return to == S::Connecting || to == S::Connected || to == S::Stopping || to == S::Error ||
               to == S::Reconnecting;
    case S::Reconnecting:
        return to == S::Starting || to == S::Disconnected;
    case S::Stopping:
        return to == S::Disconnected;
    case S::Error:
        return to == S::Starting || to == S::Disconnected;
    }
    return false;
}

bool is_busy(ConnectionState s) {
    return s != ConnectionState::Disconnected && s != ConnectionState::Error;
}

} // namespace ft
