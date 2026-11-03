#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ft {

// Команды второго экземпляра первому через Named Pipe.
struct IpcCommand {
    enum class Kind { Show, Import, Connect, Disconnect, Reconnect, Quit };

    Kind kind = Kind::Show;
    // Import - ссылка freeturn://, Connect - имя или id профиля (пусто - последний).
    std::string arg;

    bool operator==(const IpcCommand&) const = default;
};

inline constexpr std::size_t kMaxIpcMessage = 32 * 1024;

std::string encode_ipc(const IpcCommand& cmd);
std::optional<IpcCommand> decode_ipc(std::string_view msg);

// argv без имени exe. Пусто или непонятно - Show: второй запуск просто будит первый.
IpcCommand command_from_args(const std::vector<std::string>& args);

} // namespace ft
