#include "core/profile_json.h"

namespace ft {

namespace {

json::Value strings_to_json(const std::vector<std::string>& list) {
    json::Array a;
    for (const auto& s : list) {
        a.emplace_back(s);
    }
    return json::Value(std::move(a));
}

std::vector<std::string> strings_from_json(const json::Value* v) {
    std::vector<std::string> out;
    if (v == nullptr) {
        return out;
    }
    if (v->is_string()) {
        // Ядро принимает список через запятую - разрешаем и строку.
        std::string_view s = v->as_string();
        while (!s.empty()) {
            const auto comma = s.find(',');
            auto item = s.substr(0, comma);
            while (!item.empty() && item.front() == ' ') {
                item.remove_prefix(1);
            }
            while (!item.empty() && item.back() == ' ') {
                item.remove_suffix(1);
            }
            if (!item.empty()) {
                out.emplace_back(item);
            }
            if (comma == std::string_view::npos) {
                break;
            }
            s.remove_prefix(comma + 1);
        }
        return out;
    }
    for (const auto& item : v->as_array()) {
        if (item.is_string() && !item.as_string().empty()) {
            out.push_back(item.as_string());
        }
    }
    return out;
}

std::string get_str(const json::Value& o, std::string_view key, std::string_view def) {
    const auto* v = o.find(key);
    return v != nullptr && v->is_string() ? v->as_string() : std::string(def);
}

int get_int(const json::Value& o, std::string_view key, int def) {
    const auto* v = o.find(key);
    return v != nullptr ? v->as_int(def) : def;
}

bool get_bool(const json::Value& o, std::string_view key, bool def) {
    const auto* v = o.find(key);
    return v != nullptr ? v->as_bool(def) : def;
}

json::Value kcp_to_json(const KcpProfile& k) {
    return json::Object{
        {"noDelay", k.no_delay}, {"interval", k.interval},
        {"resend", k.resend},    {"nc", k.nc},
        {"sndWnd", k.snd_wnd},   {"rcvWnd", k.rcv_wnd},
        {"mtu", k.mtu},          {"ackNoDelay", k.ack_no_delay},
    };
}

KcpProfile kcp_from_json(const json::Value* v) {
    KcpProfile k;
    if (v == nullptr || !v->is_object()) {
        return k;
    }
    k.no_delay = get_int(*v, "noDelay", k.no_delay);
    k.interval = get_int(*v, "interval", k.interval);
    k.resend = get_int(*v, "resend", k.resend);
    k.nc = get_int(*v, "nc", k.nc);
    k.snd_wnd = get_int(*v, "sndWnd", k.snd_wnd);
    k.rcv_wnd = get_int(*v, "rcvWnd", k.rcv_wnd);
    k.mtu = get_int(*v, "mtu", k.mtu);
    k.ack_no_delay = get_bool(*v, "ackNoDelay", k.ack_no_delay);
    return k;
}

Profile from_core_schema(const json::Value& v) {
    Profile p;
    p.peer = get_str(v, "peer", "");
    p.provider = get_str(v, "provider", p.provider);
    p.secrets.client_id = get_str(v, "clientId", "");
    if (const auto* turn = v.find("turn"); turn != nullptr && turn->is_object()) {
        p.streams = get_int(*turn, "n", 0);
        p.transport = get_str(*turn, "transport", p.transport);
        p.turn_host = get_str(*turn, "host", "");
        p.turn_port = get_str(*turn, "port", "");
    }
    if (const auto* proxy = v.find("proxy"); proxy != nullptr && proxy->is_object()) {
        p.mode = get_str(*proxy, "mode", p.mode);
        p.listen = get_str(*proxy, "listen", p.listen);
        p.bond = get_bool(*proxy, "bond", false);
    }
    if (const auto* vk = v.find("vk"); vk != nullptr && vk->is_object()) {
        p.secrets.links = strings_from_json(vk->find("links"));
        p.streams_per_cred = get_int(*vk, "streamsPerCred", p.streams_per_cred);
        p.manual_captcha = get_bool(*vk, "manualCaptcha", false);
        p.platform = get_str(*vk, "platform", p.platform);
    }
    if (const auto* obf = v.find("obf"); obf != nullptr && obf->is_object()) {
        p.obf_profile = get_str(*obf, "profile", p.obf_profile);
        p.secrets.obf_key = get_str(*obf, "key", "");
        p.obf_timing_ms = get_int(*obf, "timingMs", 0);
    }
    if (const auto* dns = v.find("dns"); dns != nullptr && dns->is_object()) {
        p.dns_mode = get_str(*dns, "mode", p.dns_mode);
        p.dns_servers = strings_from_json(dns->find("servers"));
    }
    if (const auto* sfu = v.find("sfu"); sfu != nullptr && sfu->is_object()) {
        p.sfu_fps = get_int(*sfu, "fps", 0);
        p.sfu_batch = get_int(*sfu, "batch", 0);
    }
    p.kcp = kcp_from_json(v.find("kcp"));
    // Пустые строки в схеме ядра значат "дефолт".
    if (p.listen.empty()) {
        p.listen = std::string(kDefaultListen);
    }
    if (p.provider.empty()) {
        p.provider = std::string(kProviderVk);
    }
    if (p.mode.empty()) {
        p.mode = std::string(kModeUdp);
    }
    if (p.transport.empty()) {
        p.transport = std::string(kTransportTcp);
    }
    if (p.obf_profile.empty()) {
        p.obf_profile = std::string(kObfNone);
    }
    if (p.dns_mode.empty()) {
        p.dns_mode = "auto";
    }
    if (p.platform.empty()) {
        p.platform = "desktop";
    }
    if (p.streams_per_cred == 0) {
        p.streams_per_cred = kDefaultStreamsPerCred;
    }
    return p;
}

} // namespace

json::Value profile_to_json(const Profile& p) {
    return json::Object{
        {"version", kProfileFormatVersion},
        {"id", p.id},
        {"name", p.name},
        {"provider", p.provider},
        {"peer", p.peer},
        {"listen", p.listen},
        {"mode", p.mode},
        {"transport", p.transport},
        {"streams", p.streams},
        {"streamsPerCred", p.streams_per_cred},
        {"bond", p.bond},
        {"obfProfile", p.obf_profile},
        {"obfTimingMs", p.obf_timing_ms},
        {"dnsMode", p.dns_mode},
        {"dnsServers", strings_to_json(p.dns_servers)},
        {"manualCaptcha", p.manual_captcha},
        {"rollingCalls", p.rolling_calls},
        {"platform", p.platform},
        {"kcp", kcp_to_json(p.kcp)},
        {"sfu", json::Object{{"fps", p.sfu_fps}, {"batch", p.sfu_batch}}},
        {"turn", json::Object{{"host", p.turn_host}, {"port", p.turn_port}}},
    };
}

json::Value secrets_to_json(const ProfileSecrets& s) {
    return json::Object{
        {"obfKey", s.obf_key},
        {"clientId", s.client_id},
        {"links", strings_to_json(s.links)},
        {"wgConf", s.wg_conf},
    };
}

std::expected<Profile, std::string> profile_from_json(const json::Value& v) {
    if (!v.is_object()) {
        return std::unexpected("profile: not an object");
    }
    const int version = get_int(v, "version", kProfileFormatVersion);
    if (version > kProfileFormatVersion) {
        return std::unexpected("profile: newer format version");
    }
    Profile p;
    p.id = get_str(v, "id", "");
    p.name = sanitize_name(get_str(v, "name", ""));
    p.provider = get_str(v, "provider", p.provider);
    p.peer = get_str(v, "peer", "");
    p.listen = get_str(v, "listen", p.listen);
    p.mode = get_str(v, "mode", p.mode);
    p.transport = get_str(v, "transport", p.transport);
    p.streams = get_int(v, "streams", 0);
    p.streams_per_cred = get_int(v, "streamsPerCred", p.streams_per_cred);
    p.bond = get_bool(v, "bond", false);
    p.obf_profile = get_str(v, "obfProfile", p.obf_profile);
    p.obf_timing_ms = get_int(v, "obfTimingMs", 0);
    p.dns_mode = get_str(v, "dnsMode", p.dns_mode);
    p.dns_servers = strings_from_json(v.find("dnsServers"));
    p.manual_captcha = get_bool(v, "manualCaptcha", false);
    p.rolling_calls = get_bool(v, "rollingCalls", false);
    p.platform = get_str(v, "platform", p.platform);
    p.kcp = kcp_from_json(v.find("kcp"));
    if (const auto* sfu = v.find("sfu"); sfu != nullptr) {
        p.sfu_fps = get_int(*sfu, "fps", 0);
        p.sfu_batch = get_int(*sfu, "batch", 0);
    }
    if (const auto* turn = v.find("turn"); turn != nullptr) {
        p.turn_host = get_str(*turn, "host", "");
        p.turn_port = get_str(*turn, "port", "");
    }
    if (const auto* sec = v.find("secrets"); sec != nullptr) {
        if (auto s = secrets_from_json(*sec)) {
            p.secrets = std::move(*s);
        }
    }
    return p;
}

std::expected<ProfileSecrets, std::string> secrets_from_json(const json::Value& v) {
    if (!v.is_object()) {
        return std::unexpected("secrets: not an object");
    }
    ProfileSecrets s;
    s.obf_key = get_str(v, "obfKey", "");
    s.client_id = get_str(v, "clientId", "");
    s.links = strings_from_json(v.find("links"));
    s.wg_conf = get_str(v, "wgConf", "");
    return s;
}

std::expected<Profile, std::string> import_profile_json(std::string_view text) {
    if (text.size() > 1024 * 1024) {
        return std::unexpected("json: file too large");
    }
    auto v = json::parse(text);
    if (!v) {
        return std::unexpected("json: " + v.error().message);
    }
    if (!v->is_object()) {
        return std::unexpected("json: not an object");
    }
    const bool core_schema = v->find("obf") != nullptr || v->find("proxy") != nullptr ||
                             v->find("turn") != nullptr || v->find("vk") != nullptr;
    if (core_schema && v->find("obfProfile") == nullptr) {
        return from_core_schema(*v);
    }
    auto p = profile_from_json(*v);
    if (p) {
        // id из чужого файла не доверяем - выдаст хранилище.
        p->id.clear();
    }
    return p;
}

} // namespace ft
