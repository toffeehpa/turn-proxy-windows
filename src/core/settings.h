#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "core/json.h"

namespace ft {

struct Settings {
    std::string last_profile_id;
    bool connect_on_start = false;
    bool auto_reconnect = true;
    bool verbose_core_log = false;
    // Experimental: SFU-канал, как в Android, по умолчанию выключен.
    bool experimental_sfu = false;
    bool auto_check_updates = true;
    // Unix-время последней проверки: чаще раза в сутки не ходим.
    std::int64_t last_update_check = 0;

    bool operator==(const Settings&) const = default;
};

json::Value settings_to_json(const Settings& s);
Settings settings_from_json(const json::Value& v);
Settings parse_settings(std::string_view text);

inline constexpr std::int64_t kUpdateCheckInterval = 24 * 60 * 60;
bool update_check_due(const Settings& s, std::int64_t now_unix);

} // namespace ft
