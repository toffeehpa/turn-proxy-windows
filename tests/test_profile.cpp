#include "test.h"

#include <algorithm>

#include "core/profile.h"
#include "core/profile_json.h"
#include "helpers.h"

using namespace ft;

namespace {
bool has(const std::vector<ProfileIssue>& v, ProfileIssue i) {
    return std::ranges::find(v, i) != v.end();
}
} // namespace

TEST_CASE("profile: valid vk profile") {
    CHECK(validate(test::vk_profile()).empty());
}

TEST_CASE("profile: required fields") {
    Profile p;
    const auto issues = validate(p);
    CHECK(has(issues, ProfileIssue::MissingName));
    CHECK(has(issues, ProfileIssue::MissingPeer));
    CHECK(has(issues, ProfileIssue::MissingLinks));
}

TEST_CASE("profile: direct needs no links") {
    Profile p;
    p.name = "D";
    p.provider = "direct";
    p.peer = "vps.example.org:56000";
    CHECK(validate(p).empty());
    CHECK_EQ(p.effective_streams(), 1);
}

TEST_CASE("profile: sfu always runs one stream") {
    auto p = test::vk_profile();
    p.provider = "sfu";
    p.streams = 12;
    CHECK_EQ(p.effective_streams(), 1);
}

TEST_CASE("profile: rules mirror core validate.go") {
    auto p = test::vk_profile();
    p.secrets.obf_key = "short";
    CHECK(has(validate(p), ProfileIssue::BadObfKey));

    p = test::vk_profile();
    p.obf_profile = "none";
    p.obf_timing_ms = 20;
    CHECK(has(validate(p), ProfileIssue::ObfTimingWithoutObf));

    p = test::vk_profile();
    p.bond = true;
    CHECK(has(validate(p), ProfileIssue::BondNeedsTcp));
    p.mode = "tcp";
    CHECK(!has(validate(p), ProfileIssue::BondNeedsTcp));

    p = test::vk_profile();
    p.kcp.mtu = 1400;
    CHECK(has(validate(p), ProfileIssue::KcpNeedsTcp));
    p.mode = "tcp";
    CHECK(has(validate(p), ProfileIssue::BadKcp));

    p = test::vk_profile();
    p.provider = "turbo";
    CHECK(has(validate(p), ProfileIssue::BadProvider));

    p = test::vk_profile();
    p.secrets.links = {"https://x/call,https://y"};
    CHECK(has(validate(p), ProfileIssue::BadLink));

    p = test::vk_profile();
    p.secrets.client_id = "has space";
    CHECK(has(validate(p), ProfileIssue::BadClientId));
}

TEST_CASE("profile: host:port") {
    CHECK(is_host_port("127.0.0.1:9000"));
    CHECK(is_host_port("vps.example.org:1"));
    CHECK(is_host_port("[::1]:9000"));
    CHECK(!is_host_port("127.0.0.1"));
    CHECK(!is_host_port(":9000"));
    CHECK(!is_host_port("host:0"));
    CHECK(!is_host_port("host:65536"));
    CHECK(!is_host_port("ho st:80"));
    CHECK(!is_host_port("host:80 -debug"));
}

TEST_CASE("profile: names") {
    CHECK_EQ(sanitize_name("  a \t\n b  "), std::string("a b"));
    CHECK_EQ(sanitize_name(std::string(100, 'x')).size(), std::size_t{64});
    // Кириллица по 2 байта: обрезка не рвёт символ.
    std::string cyr;
    for (int i = 0; i < 40; ++i) {
        cyr += "\xD0\x96";
    }
    CHECK_EQ(sanitize_name(cyr).size(), std::size_t{64});
    CHECK_EQ(unique_name("Home", {"Home", "Home (2)"}), std::string("Home (3)"));
    CHECK_EQ(unique_name("", {}), std::string("FreeTurn"));
}

TEST_CASE("profile: ids") {
    const auto a = new_profile_id();
    const auto b = new_profile_id();
    CHECK(is_profile_id(a));
    CHECK(a != b);
    CHECK(!is_profile_id("../../etc"));
    CHECK(!is_profile_id("0123456789ABCDEF0123456789abcdef"));
}

TEST_CASE("profile json: roundtrip without secrets") {
    auto p = test::vk_profile();
    p.mode = "tcp";
    p.kcp.interval = 40;
    p.dns_servers = {"1.1.1.1", "8.8.8.8:53"};
    p.rolling_calls = true;
    const auto pub = json::serialize(profile_to_json(p));
    CHECK(pub.find(test::kKey) == std::string::npos);
    CHECK(pub.find("call/join") == std::string::npos);
    CHECK(pub.find(p.secrets.client_id) == std::string::npos);

    auto back = profile_from_json(*json::parse(pub));
    REQUIRE(back.has_value());
    auto sec = secrets_from_json(*json::parse(json::serialize(secrets_to_json(p.secrets))));
    REQUIRE(sec.has_value());
    back->secrets = *sec;
    CHECK(*back == p);
}

TEST_CASE("profile json: newer version rejected") {
    CHECK(!profile_from_json(*json::parse(R"({"version":99})")).has_value());
}

TEST_CASE("profile json: import core ClientJSON schema") {
    const auto text = R"({
      "peer": "203.0.113.7:56000", "clientId": "cid123456", "provider": "vk", "routes": false,
      "turn": {"n": 8, "transport": "udp", "host": "", "port": ""},
      "proxy": {"mode": "tcp", "listen": "127.0.0.1:9100", "bond": true},
      "vk": {"links": ["https://example.invalid/call/join/A", "https://example.invalid/call/join/B"],
             "streamsPerCred": 6, "manualCaptcha": true, "platform": "mobile"},
      "obf": {"profile": "rtpopus2", "key": ")" +
                      test::kKey + R"(", "timingMs": 20},
      "dns": {"mode": "doh", "servers": []},
      "log": {"debug": false},
      "kcp": {"noDelay": 1, "interval": 40, "resend": 2, "nc": 1, "sndWnd": 256, "rcvWnd": 256, "mtu": 1200, "ackNoDelay": false},
      "tunnel": {"mode": "", "config": "", "mtu": 0}
    })";
    auto p = import_profile_json(text);
    REQUIRE(p.has_value());
    CHECK_EQ(p->peer, std::string("203.0.113.7:56000"));
    CHECK_EQ(p->streams, 8);
    CHECK_EQ(p->transport, std::string("udp"));
    CHECK_EQ(p->mode, std::string("tcp"));
    CHECK_EQ(p->listen, std::string("127.0.0.1:9100"));
    CHECK(p->bond);
    CHECK_EQ(p->secrets.links.size(), std::size_t{2});
    CHECK_EQ(p->streams_per_cred, 6);
    CHECK(p->manual_captcha);
    CHECK_EQ(p->platform, std::string("mobile"));
    CHECK_EQ(p->obf_profile, std::string("rtpopus2"));
    CHECK_EQ(p->secrets.obf_key, test::kKey);
    CHECK_EQ(p->obf_timing_ms, 20);
    CHECK_EQ(p->dns_mode, std::string("doh"));
    CHECK_EQ(p->kcp.interval, 40);
    CHECK(!p->kcp.ack_no_delay);
    CHECK_EQ(p->secrets.client_id, std::string("cid123456"));
    CHECK(p->id.empty());
}

TEST_CASE("profile json: import own format drops foreign id") {
    auto src = test::vk_profile();
    auto v = profile_to_json(src);
    v.set("secrets", secrets_to_json(src.secrets));
    auto p = import_profile_json(json::serialize(v));
    REQUIRE(p.has_value());
    CHECK(p->id.empty());
    CHECK_EQ(p->secrets.obf_key, test::kKey);
    CHECK(!import_profile_json("[1]").has_value());
    CHECK(!import_profile_json("{").has_value());
}
