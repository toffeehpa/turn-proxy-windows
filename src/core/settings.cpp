#include "core/settings.h"

namespace ft {

json::Value settings_to_json(const Settings& s) {
    return json::Object{
        {"version", 1},
        {"lastProfileId", s.last_profile_id},
        {"connectOnStart", s.connect_on_start},
        {"autoReconnect", s.auto_reconnect},
        {"verboseCoreLog", s.verbose_core_log},
        {"experimental", json::Object{{"sfu", s.experimental_sfu}}},
        {"updates", json::Object{{"autoCheck", s.auto_check_updates},
                                 {"lastCheck", static_cast<double>(s.last_update_check)}}},
    };
}

Settings settings_from_json(const json::Value& v) {
    Settings s;
    auto str = [&](std::string_view key) {
        const auto* x = v.find(key);
        return x != nullptr ? x->as_string() : std::string();
    };
    auto flag = [](const json::Value* x, bool def) {
        return x != nullptr ? x->as_bool(def) : def;
    };
    s.last_profile_id = str("lastProfileId");
    s.connect_on_start = flag(v.find("connectOnStart"), s.connect_on_start);
    s.auto_reconnect = flag(v.find("autoReconnect"), s.auto_reconnect);
    s.verbose_core_log = flag(v.find("verboseCoreLog"), s.verbose_core_log);
    if (const auto* e = v.find("experimental")) {
        s.experimental_sfu = flag(e->find("sfu"), s.experimental_sfu);
    }
    if (const auto* u = v.find("updates")) {
        s.auto_check_updates = flag(u->find("autoCheck"), s.auto_check_updates);
        if (const auto* t = u->find("lastCheck"); t != nullptr && t->is_number() && t->as_double() >= 0) {
            s.last_update_check = static_cast<std::int64_t>(t->as_double());
        }
    }
    return s;
}

Settings parse_settings(std::string_view text) {
    auto v = json::parse(text);
    if (!v || !v->is_object()) {
        return {};
    }
    return settings_from_json(*v);
}

bool update_check_due(const Settings& s, std::int64_t now_unix) {
    // Часы ушли назад - тоже проверяем, иначе застрянем надолго.
    return s.auto_check_updates &&
           (now_unix - s.last_update_check >= kUpdateCheckInterval || now_unix < s.last_update_check);
}

} // namespace ft
