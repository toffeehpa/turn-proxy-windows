#include "core/freeturn_uri.h"

#include "core/base64.h"
#include "core/json.h"

namespace ft {

namespace {

std::string trim(std::string_view s) {
    while (!s.empty() && static_cast<unsigned char>(s.front()) <= 0x20) {
        s.remove_prefix(1);
    }
    while (!s.empty() && static_cast<unsigned char>(s.back()) <= 0x20) {
        s.remove_suffix(1);
    }
    return std::string(s);
}

std::string str(const json::Value& o, std::string_view key) {
    const auto* v = o.find(key);
    return v != nullptr && v->is_string() ? v->as_string() : std::string();
}

int num(const json::Value& o, std::string_view key) {
    const auto* v = o.find(key);
    return v != nullptr ? v->as_int(0) : 0;
}

bool flag(const json::Value& o, std::string_view key) {
    const auto* v = o.find(key);
    return v != nullptr && v->as_bool(false);
}

std::vector<std::string> split_commas(std::string_view s) {
    std::vector<std::string> out;
    while (!s.empty()) {
        const auto comma = s.find(',');
        auto item = trim(s.substr(0, comma));
        if (!item.empty()) {
            out.push_back(std::move(item));
        }
        if (comma == std::string_view::npos) {
            break;
        }
        s.remove_prefix(comma + 1);
    }
    return out;
}

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

} // namespace

std::string_view to_string(UriError e) {
    switch (e) {
    case UriError::BadScheme:
        return "bad_scheme";
    case UriError::Empty:
        return "empty";
    case UriError::TooLong:
        return "too_long";
    case UriError::BadBase64:
        return "bad_base64";
    case UriError::BadJson:
        return "bad_json";
    case UriError::UnsupportedVersion:
        return "unsupported_version";
    case UriError::MissingProvider:
        return "missing_provider";
    case UriError::MissingPeer:
        return "missing_peer";
    }
    return "unknown";
}

std::expected<Profile, UriError> parse_freeturn_uri(std::string_view input) {
    const std::string uri = trim(input);
    if (uri.size() > kMaxUriLength) {
        return std::unexpected(UriError::TooLong);
    }
    if (!uri.starts_with(kUriScheme)) {
        return std::unexpected(UriError::BadScheme);
    }
    std::string_view payload = std::string_view(uri).substr(kUriScheme.size());
    // Браузеры и мессенджеры любят дописывать слэш.
    while (!payload.empty() && payload.back() == '/') {
        payload.remove_suffix(1);
    }
    if (payload.empty()) {
        return std::unexpected(UriError::Empty);
    }
    auto raw = base64::decode(payload);
    if (!raw) {
        return std::unexpected(UriError::BadBase64);
    }
    auto v = json::parse(*raw);
    if (!v || !v->is_object()) {
        return std::unexpected(UriError::BadJson);
    }
    if (num(*v, "v") != kUriVersion) {
        return std::unexpected(UriError::UnsupportedVersion);
    }

    Profile p;
    p.provider = str(*v, "provider");
    p.peer = str(*v, "peer");
    if (p.provider.empty()) {
        return std::unexpected(UriError::MissingProvider);
    }
    if (p.peer.empty()) {
        return std::unexpected(UriError::MissingPeer);
    }
    if (auto t = str(*v, "transport"); !t.empty()) {
        p.transport = std::move(t);
    }
    if (auto m = str(*v, "mode"); !m.empty()) {
        p.mode = std::move(m);
    }
    p.bond = flag(*v, "bond");
    if (auto o = str(*v, "obf"); !o.empty()) {
        p.obf_profile = std::move(o);
        p.secrets.obf_key = str(*v, "key");
        if (p.obf_profile != kObfNone) {
            p.obf_timing_ms = num(*v, "timing");
        }
    }
    p.streams = num(*v, "n");
    if (const int spc = num(*v, "spc"); spc > 0) {
        p.streams_per_cred = spc;
    }
    p.secrets.client_id = str(*v, "cid");
    if (auto l = str(*v, "listen"); !l.empty()) {
        p.listen = std::move(l);
    }
    if (auto d = str(*v, "dns"); !d.empty()) {
        p.dns_mode = std::move(d);
    }
    p.dns_servers = split_commas(str(*v, "dnss"));
    p.manual_captcha = flag(*v, "mcap");
    if (const auto* k = v->find("kcp"); k != nullptr && k->is_object()) {
        p.kcp.no_delay = num(*k, "nodelay");
        p.kcp.interval = num(*k, "interval");
        p.kcp.resend = num(*k, "resend");
        p.kcp.nc = num(*k, "nc");
        p.kcp.snd_wnd = num(*k, "sndwnd");
        p.kcp.rcv_wnd = num(*k, "rcvwnd");
        p.kcp.mtu = num(*k, "mtu");
        p.kcp.ack_no_delay = flag(*k, "acknodelay");
    }
    p.name = sanitize_name(str(*v, "name"));
    p.secrets.links = split_commas(str(*v, "vk"));
    p.secrets.wg_conf = str(*v, "wg");
    return p;
}

std::string build_freeturn_uri(const Profile& p) {
    json::Value o = json::Object{{"v", kUriVersion}, {"provider", p.provider}, {"peer", p.peer}};
    if (p.transport != kTransportTcp) {
        o.set("transport", p.transport);
    }
    if (p.mode != kModeUdp) {
        o.set("mode", p.mode);
    }
    if (p.bond) {
        o.set("bond", true);
    }
    if (p.obf_profile != kObfNone && !p.obf_profile.empty()) {
        o.set("obf", p.obf_profile);
        o.set("key", p.secrets.obf_key);
        if (p.obf_timing_ms > 0) {
            o.set("timing", p.obf_timing_ms);
        }
    }
    if (p.streams > 0) {
        o.set("n", p.streams);
    }
    if (p.streams_per_cred != kDefaultStreamsPerCred) {
        o.set("spc", p.streams_per_cred);
    }
    if (!p.secrets.client_id.empty()) {
        o.set("cid", p.secrets.client_id);
    }
    if (p.listen != kDefaultListen) {
        o.set("listen", p.listen);
    }
    if (p.dns_mode != "auto") {
        o.set("dns", p.dns_mode);
    }
    if (!p.dns_servers.empty()) {
        o.set("dnss", join_commas(p.dns_servers));
    }
    if (p.manual_captcha) {
        o.set("mcap", true);
    }
    if (p.mode == kModeTcp && p.kcp != KcpProfile{}) {
        o.set("kcp", json::Object{{"nodelay", p.kcp.no_delay},
                                  {"interval", p.kcp.interval},
                                  {"resend", p.kcp.resend},
                                  {"nc", p.kcp.nc},
                                  {"sndwnd", p.kcp.snd_wnd},
                                  {"rcvwnd", p.kcp.rcv_wnd},
                                  {"mtu", p.kcp.mtu},
                                  {"acknodelay", p.kcp.ack_no_delay}});
    }
    if (!p.secrets.wg_conf.empty()) {
        o.set("wg", p.secrets.wg_conf);
    }
    if (!p.name.empty()) {
        o.set("name", p.name);
    }
    // Ссылку на звонок в share не кладём: по uri.md она у каждого клиента своя.
    return std::string(kUriScheme) + base64::encode_url(json::serialize(o, -1));
}

} // namespace ft
