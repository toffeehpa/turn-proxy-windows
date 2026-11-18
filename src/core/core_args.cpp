#include "core/core_args.h"

#include <algorithm>
#include <array>

namespace ft {

namespace {

std::string join_commas(const std::vector<std::string>& list) {
    std::string out;
    for (const auto& s : list) {
        if (!out.empty()) {
            out.push_back(',');
        }
        out += s;
    }
    return out;
}

constexpr std::array<std::string_view, 5> kSecretFlags = {"-obf-key", "-client-id", "-links", "-link",
                                                          "-tun-conf"};

} // namespace

std::vector<std::string> build_core_args(const Profile& p, const CoreArgsOptions& opts) {
    std::vector<std::string> args;
    auto add = [&](std::string_view flag, std::string value) {
        args.emplace_back(flag);
        args.push_back(std::move(value));
    };

    add("-peer", p.peer);
    if (p.provider != kProviderDirect && !p.secrets.links.empty()) {
        add("-links", join_commas(p.secrets.links));
    }
    if (p.provider != kProviderVk) {
        add("-provider", p.provider);
    }
    if (p.listen != kDefaultListen) {
        add("-listen", p.listen);
    }
    if (!p.turn_host.empty()) {
        add("-turn", p.turn_host);
    }
    if (!p.turn_port.empty()) {
        add("-port", p.turn_port);
    }
    if (p.streams > 0) {
        add("-n", std::to_string(p.streams));
    }
    if (p.provider == kProviderVk && p.streams_per_cred != kDefaultStreamsPerCred) {
        add("-streams-per-cred", std::to_string(p.streams_per_cred));
    }
    if (p.transport == kTransportUdp) {
        add("-transport", std::string(kTransportUdp));
    }
    if (p.mode != kModeUdp) {
        add("-mode", p.mode);
    }
    if (p.bond) {
        args.emplace_back("-bond");
    }
    if (p.mode == kModeTcp) {
        const KcpProfile def;
        const auto& k = p.kcp;
        auto add_int = [&](std::string_view flag, int v, int d) {
            if (v != d) {
                add(flag, std::to_string(v));
            }
        };
        add_int("-kcp-nodelay", k.no_delay, def.no_delay);
        add_int("-kcp-interval", k.interval, def.interval);
        add_int("-kcp-resend", k.resend, def.resend);
        add_int("-kcp-nc", k.nc, def.nc);
        add_int("-kcp-sndwnd", k.snd_wnd, def.snd_wnd);
        add_int("-kcp-rcvwnd", k.rcv_wnd, def.rcv_wnd);
        add_int("-kcp-mtu", k.mtu, def.mtu);
        if (k.ack_no_delay != def.ack_no_delay) {
            // Булев флаг Go принимает только в форме -flag=value.
            args.emplace_back(k.ack_no_delay ? "-kcp-acknodelay=true" : "-kcp-acknodelay=false");
        }
    }
    if (p.obf_profile != kObfNone) {
        add("-obf-profile", p.obf_profile);
        add("-obf-key", p.secrets.obf_key);
        if (p.obf_timing_ms > 0) {
            add("-obf-timing", std::to_string(p.obf_timing_ms) + "ms");
        }
    }
    if (p.provider == kProviderVk) {
        if (p.manual_captcha) {
            args.emplace_back("-manual-captcha");
        }
        if (p.platform != "desktop") {
            add("-platform", p.platform);
        }
    }
    if (p.provider == kProviderSfu) {
        if (p.sfu_fps > 0) {
            add("-sfu-fps", std::to_string(p.sfu_fps));
        }
        if (p.sfu_batch > 0) {
            add("-sfu-batch", std::to_string(p.sfu_batch));
        }
    }
    if (p.dns_mode != "auto") {
        add("-dns-mode", p.dns_mode);
    }
    if (!p.dns_servers.empty()) {
        add("-dns-servers", join_commas(p.dns_servers));
    }
    if (!p.secrets.client_id.empty()) {
        add("-client-id", p.secrets.client_id);
    }
    // Туннель из ссылки поднимает ядро через Wintun; маршруты к TURN тогда не нужны - сокеты
    // ядра оно само держит на физической сети.
    if (!p.secrets.wg_conf.empty() && p.mode == kModeUdp) {
        args.emplace_back("-tun");
        add("-tun-conf", p.secrets.wg_conf);
    } else if (opts.routes) {
        args.emplace_back("-routes");
    }
    if (opts.debug) {
        args.emplace_back("-debug");
    }
    return args;
}

std::vector<std::string> redact_core_args(const std::vector<std::string>& args) {
    std::vector<std::string> out;
    out.reserve(args.size());
    bool hide_next = false;
    for (const auto& a : args) {
        if (hide_next) {
            out.emplace_back("***");
            hide_next = false;
            continue;
        }
        const auto eq = a.find('=');
        const std::string_view name = std::string_view(a).substr(0, eq);
        const bool secret =
            std::ranges::find(kSecretFlags, name) != kSecretFlags.end() ||
            (name.starts_with("--") && std::ranges::find(kSecretFlags, name.substr(1)) != kSecretFlags.end());
        if (secret && eq != std::string::npos) {
            out.push_back(std::string(name) + "=***");
            continue;
        }
        if (secret) {
            hide_next = true;
        }
        // freeturn:// несёт ключ внутри - целиком тоже секрет.
        out.push_back(a.starts_with("freeturn://") ? std::string("freeturn://***") : a);
    }
    return out;
}

std::string quote_windows_arg(std::string_view arg) {
    if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string_view::npos) {
        return std::string(arg);
    }
    std::string out = "\"";
    std::size_t backslashes = 0;
    for (const char c : arg) {
        if (c == '\\') {
            ++backslashes;
            continue;
        }
        if (c == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out.push_back('"');
        } else {
            out.append(backslashes, '\\');
            out.push_back(c);
        }
        backslashes = 0;
    }
    // Слэши перед закрывающей кавычкой удваиваем, иначе они её экранируют.
    out.append(backslashes * 2, '\\');
    out.push_back('"');
    return out;
}

std::string join_windows_command_line(const std::vector<std::string>& args) {
    std::string out;
    for (const auto& a : args) {
        if (!out.empty()) {
            out.push_back(' ');
        }
        out += quote_windows_arg(a);
    }
    return out;
}

std::vector<std::string> split_windows_command_line(std::string_view s) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
            ++i;
        }
        if (i >= s.size()) {
            break;
        }
        std::string cur;
        bool quoted = false;
        while (i < s.size() && (quoted || (s[i] != ' ' && s[i] != '\t'))) {
            std::size_t backslashes = 0;
            while (i < s.size() && s[i] == '\\') {
                ++backslashes;
                ++i;
            }
            if (i < s.size() && s[i] == '"') {
                cur.append(backslashes / 2, '\\');
                if (backslashes % 2 == 1) {
                    cur.push_back('"');
                    ++i;
                    continue;
                }
                if (quoted && i + 1 < s.size() && s[i + 1] == '"') {
                    cur.push_back('"');
                    i += 2;
                    continue;
                }
                quoted = !quoted;
                ++i;
                continue;
            }
            cur.append(backslashes, '\\');
            if (i < s.size() && (quoted || (s[i] != ' ' && s[i] != '\t'))) {
                cur.push_back(s[i]);
                ++i;
            }
        }
        out.push_back(std::move(cur));
    }
    return out;
}

} // namespace ft
