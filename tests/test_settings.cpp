#include "test.h"

#include "core/settings.h"

using namespace ft;

TEST_CASE("settings: defaults are safe") {
    const Settings s;
    CHECK(!s.experimental_sfu);
    CHECK(s.auto_check_updates);
    CHECK(!s.connect_on_start);
    CHECK(parse_settings("garbage") == Settings{});
}

TEST_CASE("settings: roundtrip") {
    Settings s;
    s.last_profile_id = "0123456789abcdef0123456789abcdef";
    s.experimental_sfu = true;
    s.verbose_core_log = true;
    s.last_update_check = 1'800'000'000;
    CHECK(parse_settings(json::serialize(settings_to_json(s))) == s);
}

TEST_CASE("settings: update check once a day") {
    Settings s;
    s.last_update_check = 1'000'000;
    CHECK(!update_check_due(s, 1'000'000 + 3600));
    CHECK(update_check_due(s, 1'000'000 + kUpdateCheckInterval));
    CHECK(update_check_due(s, 10));
    s.auto_check_updates = false;
    CHECK(!update_check_due(s, 1'000'000 + 10 * kUpdateCheckInterval));
}
