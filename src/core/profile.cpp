#include "core/profile.h"

#include <algorithm>
#include <charconv>
#include <format>
#include <random>

namespace ft {

namespace {

bool parse_port(std::string_view s) {
    if (s.empty() || s.size() > 5) {
        return false;
    }
    int port = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), port);
    return ec == std::errc() && ptr == s.data() + s.size() && port >= 1 && port <= 65535;
}

bool is_host_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
           c == '-' || c == '_';
}

// Ссылки на звонки проверяем мягко: ядро само разберётся, нам важно не пустить мусор в командную строку.
bool is_link(std::string_view s) {
    if (!s.starts_with("https://") || s.size() > 512) {
        return false;
    }
    return std::ranges::none_of(s, [](char c) {
        return static_cast<unsigned char>(c) <= 0x20 || c == ',' || c == '"' ||
               static_cast<unsigned char>(c) >= 0x7F;
    });
}

bool is_token(std::string_view s, std::size_t max_len) {
    if (s.empty() || s.size() > max_len) {
        return false;
    }
    return std::ranges::all_of(s, [](char c) {
        return static_cast<unsigned char>(c) > 0x20 && static_cast<unsigned char>(c) < 0x7F && c != '"' &&
               c != ',';
    });
}

} // namespace

int Profile::effective_streams() const {
    if (streams > 0 && provider != kProviderSfu) {
        return streams;
    }
    // SFU ядро всегда ведёт одним потоком: видеотрек один на клиента.
    if (provider == kProviderSfu) {
        return 1;
    }
    return provider == kProviderDirect ? kDefaultDirectStreams : kDefaultStreams;
}

bool is_host_port(std::string_view s) {
    if (s.empty() || s.size() > 260) {
        return false;
    }
    if (s.front() == '[') {
        const auto close = s.find(']');
        if (close == std::string_view::npos || close + 1 >= s.size() || s[close + 1] != ':') {
            return false;
        }
        const auto host = s.substr(1, close - 1);
        if (host.empty() || !std::ranges::all_of(host, [](char c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') ||
                       c == ':' || c == '.';
            })) {
            return false;
        }
        return parse_port(s.substr(close + 2));
    }
    const auto colon = s.rfind(':');
    if (colon == std::string_view::npos || colon == 0) {
        return false;
    }
    const auto host = s.substr(0, colon);
    return std::ranges::all_of(host, is_host_char) && parse_port(s.substr(colon + 1));
}

bool is_hex_key(std::string_view s) {
    return s.size() == 64 && std::ranges::all_of(s, [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
           });
}

std::string_view to_string(ProfileIssue issue) {
    switch (issue) {
    case ProfileIssue::MissingName:
        return "missing_name";
    case ProfileIssue::MissingPeer:
        return "missing_peer";
    case ProfileIssue::BadPeer:
        return "bad_peer";
    case ProfileIssue::BadListen:
        return "bad_listen";
    case ProfileIssue::BadProvider:
        return "bad_provider";
    case ProfileIssue::MissingLinks:
        return "missing_links";
    case ProfileIssue::BadLink:
        return "bad_link";
    case ProfileIssue::BadMode:
        return "bad_mode";
    case ProfileIssue::BadTransport:
        return "bad_transport";
    case ProfileIssue::BadObfProfile:
        return "bad_obf_profile";
    case ProfileIssue::BadObfKey:
        return "bad_obf_key";
    case ProfileIssue::ObfTimingWithoutObf:
        return "obf_timing_without_obf";
    case ProfileIssue::BadStreams:
        return "bad_streams";
    case ProfileIssue::BadStreamsPerCred:
        return "bad_streams_per_cred";
    case ProfileIssue::BadPlatform:
        return "bad_platform";
    case ProfileIssue::BadDnsMode:
        return "bad_dns_mode";
    case ProfileIssue::BadDnsServer:
        return "bad_dns_server";
    case ProfileIssue::BondNeedsTcp:
        return "bond_needs_tcp";
    case ProfileIssue::KcpNeedsTcp:
        return "kcp_needs_tcp";
    case ProfileIssue::BadKcp:
        return "bad_kcp";
    case ProfileIssue::BadSfu:
        return "bad_sfu";
    case ProfileIssue::BadClientId:
        return "bad_client_id";
    }
    return "unknown";
}

std::vector<ProfileIssue> validate(const Profile& p) {
    std::vector<ProfileIssue> out;
    auto add = [&](ProfileIssue i) {
        out.push_back(i);
    };

    if (sanitize_name(p.name).empty()) {
        add(ProfileIssue::MissingName);
    }
    if (p.peer.empty()) {
        add(ProfileIssue::MissingPeer);
    } else if (!is_host_port(p.peer)) {
        add(ProfileIssue::BadPeer);
    }
    if (!is_host_port(p.listen)) {
        add(ProfileIssue::BadListen);
    }

    const bool vk = p.provider == kProviderVk;
    const bool sfu = p.provider == kProviderSfu;
    if (!vk && !sfu && p.provider != kProviderDirect) {
        add(ProfileIssue::BadProvider);
    }
    if ((vk || sfu) && p.secrets.links.empty()) {
        add(ProfileIssue::MissingLinks);
    }
    if (std::ranges::any_of(p.secrets.links, [](const std::string& l) { return !is_link(l); })) {
        add(ProfileIssue::BadLink);
    }

    const bool tcp = p.mode == kModeTcp;
    if (!tcp && p.mode != kModeUdp) {
        add(ProfileIssue::BadMode);
    }
    if (p.transport != kTransportTcp && p.transport != kTransportUdp) {
        add(ProfileIssue::BadTransport);
    }

    const bool obf = p.obf_profile != kObfNone;
    if (p.obf_profile != kObfNone && p.obf_profile != "rtpopus" && p.obf_profile != "rtpopus2" &&
        p.obf_profile != "rtpopus3") {
        add(ProfileIssue::BadObfProfile);
    }
    if (obf && !is_hex_key(p.secrets.obf_key)) {
        add(ProfileIssue::BadObfKey);
    }
    if (p.obf_timing_ms < 0 || (p.obf_timing_ms > 0 && !obf)) {
        add(ProfileIssue::ObfTimingWithoutObf);
    }

    if (p.streams < 0 || p.streams > kBondMaxLanes) {
        add(ProfileIssue::BadStreams);
    }
    if (vk && p.streams_per_cred <= 0) {
        add(ProfileIssue::BadStreamsPerCred);
    }
    if (vk && p.platform != "desktop" && p.platform != "mobile") {
        add(ProfileIssue::BadPlatform);
    }
    if (p.dns_mode != "plain" && p.dns_mode != "doh" && p.dns_mode != "auto") {
        add(ProfileIssue::BadDnsMode);
    }
    if (std::ranges::any_of(p.dns_servers, [](const std::string& s) { return !is_token(s, 64); })) {
        add(ProfileIssue::BadDnsServer);
    }

    if (p.bond) {
        const int groups = vk ? std::max<int>(static_cast<int>(p.secrets.links.size()), 1) : 1;
        if (!tcp || p.effective_streams() > kBondMaxLanes / groups) {
            add(ProfileIssue::BondNeedsTcp);
        }
    }
    if (!tcp && p.kcp != KcpProfile{}) {
        add(ProfileIssue::KcpNeedsTcp);
    }
    if (tcp) {
        const auto& k = p.kcp;
        if ((k.no_delay != 0 && k.no_delay != 1) || (k.nc != 0 && k.nc != 1) || k.interval <= 0 ||
            k.resend < 0 || k.snd_wnd <= 0 || k.rcv_wnd <= 0 || k.mtu < 300 || k.mtu > 1350) {
            add(ProfileIssue::BadKcp);
        }
    }
    if (p.sfu_fps < 0 || p.sfu_fps > 240 || p.sfu_batch < 0 || p.sfu_batch > 256) {
        add(ProfileIssue::BadSfu);
    }
    if (!p.secrets.client_id.empty() && !is_token(p.secrets.client_id, 128)) {
        add(ProfileIssue::BadClientId);
    }
    return out;
}

std::string sanitize_name(std::string_view name) {
    std::string out;
    out.reserve(name.size());
    bool space = false;
    for (const char c : name) {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20 || u == 0x7F || c == ' ') {
            space = !out.empty();
            continue;
        }
        if (space) {
            out.push_back(' ');
            space = false;
        }
        out.push_back(c);
    }
    if (out.size() > 64) {
        std::size_t cut = 64;
        // Не режем посреди UTF-8 символа.
        while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) {
            --cut;
        }
        out.resize(cut);
    }
    return out;
}

std::string unique_name(std::string_view base, const std::vector<std::string>& taken) {
    std::string clean = sanitize_name(base);
    if (clean.empty()) {
        clean = "FreeTurn";
    }
    auto used = [&](const std::string& n) {
        return std::ranges::find(taken, n) != taken.end();
    };
    if (!used(clean)) {
        return clean;
    }
    for (int i = 2;; ++i) {
        auto candidate = std::format("{} ({})", clean, i);
        if (!used(candidate)) {
            return candidate;
        }
    }
}

std::string new_profile_id() {
    std::random_device rd;
    static constexpr char kHex[] = "0123456789abcdef";
    std::string id;
    id.reserve(32);
    for (int i = 0; i < 8; ++i) {
        const auto v = rd();
        for (int s = 0; s < 4; ++s) {
            id.push_back(kHex[(v >> (s * 4)) & 0xF]);
        }
    }
    return id;
}

bool is_profile_id(std::string_view id) {
    return id.size() == 32 &&
           std::ranges::all_of(id, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

} // namespace ft
