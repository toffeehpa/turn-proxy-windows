#include "core/ipc_protocol.h"

#include <algorithm>
#include <array>
#include <utility>

namespace ft {

namespace {

constexpr std::string_view kMagic = "FTIPC1 ";

constexpr std::array<std::pair<IpcCommand::Kind, std::string_view>, 6> kNames = {{
    {IpcCommand::Kind::Show, "show"},
    {IpcCommand::Kind::Import, "import"},
    {IpcCommand::Kind::Connect, "connect"},
    {IpcCommand::Kind::Disconnect, "disconnect"},
    {IpcCommand::Kind::Reconnect, "reconnect"},
    {IpcCommand::Kind::Quit, "quit"},
}};

bool clean(std::string_view s) {
    return std::ranges::none_of(s, [](char c) { return c == '\n' || c == '\r' || c == '\0'; });
}

} // namespace

std::string encode_ipc(const IpcCommand& cmd) {
    std::string out(kMagic);
    for (const auto& [kind, name] : kNames) {
        if (kind == cmd.kind) {
            out += name;
        }
    }
    if (!cmd.arg.empty()) {
        out.push_back(' ');
        out += cmd.arg;
    }
    return out;
}

std::optional<IpcCommand> decode_ipc(std::string_view msg) {
    if (msg.size() > kMaxIpcMessage || !msg.starts_with(kMagic) || !clean(msg)) {
        return std::nullopt;
    }
    msg.remove_prefix(kMagic.size());
    const auto space = msg.find(' ');
    const auto name = msg.substr(0, space);
    const auto arg = space == std::string_view::npos ? std::string_view{} : msg.substr(space + 1);
    for (const auto& [kind, n] : kNames) {
        if (n != name) {
            continue;
        }
        IpcCommand cmd{kind, std::string(arg)};
        if (kind == IpcCommand::Kind::Import && !cmd.arg.starts_with("freeturn://")) {
            return std::nullopt;
        }
        if ((kind == IpcCommand::Kind::Show || kind == IpcCommand::Kind::Disconnect ||
             kind == IpcCommand::Kind::Reconnect || kind == IpcCommand::Kind::Quit) &&
            !cmd.arg.empty()) {
            return std::nullopt;
        }
        return cmd;
    }
    return std::nullopt;
}

IpcCommand command_from_args(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {};
    }
    const std::string& first = args.front();
    if (first.starts_with("freeturn://")) {
        return {IpcCommand::Kind::Import, first};
    }
    if (first == "--import" && args.size() > 1 && args[1].starts_with("freeturn://")) {
        return {IpcCommand::Kind::Import, args[1]};
    }
    if (first == "--connect") {
        return {IpcCommand::Kind::Connect, args.size() > 1 ? args[1] : std::string()};
    }
    if (first == "--disconnect") {
        return {IpcCommand::Kind::Disconnect, {}};
    }
    if (first == "--reconnect") {
        return {IpcCommand::Kind::Reconnect, {}};
    }
    if (first == "--quit") {
        return {IpcCommand::Kind::Quit, {}};
    }
    return {};
}

} // namespace ft
