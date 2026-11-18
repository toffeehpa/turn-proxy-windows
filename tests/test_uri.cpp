#include "test.h"

#include "core/base64.h"
#include "core/freeturn_uri.h"
#include "helpers.h"

using namespace ft;

namespace {
std::string link(std::string_view json) {
    return "freeturn://" + base64::encode_url(json);
}
} // namespace

TEST_CASE("uri: docs example") {
    auto p = parse_freeturn_uri(
        link(R"({"v":1,"provider":"vk","peer":"1.2.3.4:56000","transport":"tcp","mode":"udp",)"
             R"("obf":"rtpopus3","key":")" +
             test::kKey + R"(","n":10,"cid":"a1b2c3","name":"RU-Server"})"));
    REQUIRE(p.has_value());
    CHECK_EQ(p->provider, std::string("vk"));
    CHECK_EQ(p->peer, std::string("1.2.3.4:56000"));
    CHECK_EQ(p->obf_profile, std::string("rtpopus3"));
    CHECK_EQ(p->secrets.obf_key, test::kKey);
    CHECK_EQ(p->streams, 10);
    CHECK_EQ(p->secrets.client_id, std::string("a1b2c3"));
    CHECK_EQ(p->name, std::string("RU-Server"));
    CHECK(p->secrets.links.empty());
    CHECK_EQ(p->listen, std::string("127.0.0.1:9000"));
}

TEST_CASE("uri: optional fields and vk link") {
    auto p = parse_freeturn_uri(
        "  " +
        link(
            R"({"v":1,"provider":"direct","peer":"h:1","mode":"tcp","bond":true,"spc":4,"listen":"127.0.0.1:9100",)"
            R"("dns":"doh","dnss":"1.1.1.1, 8.8.8.8","mcap":true,"vk":"https://example.invalid/call/join/Z",)"
            R"("kcp":{"nodelay":0,"interval":40,"resend":0,"nc":0,"sndwnd":256,"rcvwnd":256,"mtu":1000,"acknodelay":false},)"
            R"("future":"ignored"})") +
        "/\n");
    REQUIRE(p.has_value());
    CHECK_EQ(p->mode, std::string("tcp"));
    CHECK(p->bond);
    CHECK_EQ(p->streams_per_cred, 4);
    CHECK_EQ(p->dns_servers.size(), std::size_t{2});
    CHECK_EQ(p->dns_servers[1], std::string("8.8.8.8"));
    CHECK(p->manual_captcha);
    CHECK_EQ(p->secrets.links.size(), std::size_t{1});
    CHECK_EQ(p->kcp.mtu, 1000);
    CHECK(!p->kcp.ack_no_delay);
}

TEST_CASE("uri: errors") {
    CHECK(parse_freeturn_uri("http://x").error() == UriError::BadScheme);
    CHECK(parse_freeturn_uri("freeturn://").error() == UriError::Empty);
    CHECK(parse_freeturn_uri("freeturn://***").error() == UriError::BadBase64);
    CHECK(parse_freeturn_uri(link("not json")).error() == UriError::BadJson);
    CHECK(parse_freeturn_uri(link(R"({"v":2,"provider":"vk","peer":"h:1"})")).error() ==
          UriError::UnsupportedVersion);
    CHECK(parse_freeturn_uri(link(R"({"v":1,"peer":"h:1"})")).error() == UriError::MissingProvider);
    CHECK(parse_freeturn_uri(link(R"({"v":1,"provider":"vk"})")).error() == UriError::MissingPeer);
    CHECK(parse_freeturn_uri("freeturn://" + std::string(kMaxUriLength, 'A')).error() == UriError::TooLong);
}

TEST_CASE("uri: build and parse back") {
    auto p = test::vk_profile("Office");
    p.mode = "tcp";
    p.kcp.interval = 40;
    p.streams = 6;
    p.obf_timing_ms = 20;
    const auto uri = build_freeturn_uri(p);
    auto back = parse_freeturn_uri(uri);
    REQUIRE(back.has_value());
    CHECK_EQ(back->peer, p.peer);
    CHECK_EQ(back->mode, p.mode);
    CHECK_EQ(back->kcp.interval, 40);
    CHECK_EQ(back->streams, 6);
    CHECK_EQ(back->obf_timing_ms, 20);
    CHECK_EQ(back->secrets.obf_key, p.secrets.obf_key);
    CHECK_EQ(back->name, std::string("Office"));
    // Ссылка на звонок у каждого своя - в share не уходит.
    CHECK(back->secrets.links.empty());
}

TEST_CASE("uri: wg config survives import and share") {
    auto p = parse_freeturn_uri(
        link(R"({"v":1,"provider":"vk","peer":"1.2.3.4:56000","wg":"[Interface]\nPrivateKey = k"})"));
    REQUIRE(p.has_value());
    CHECK_EQ(p->secrets.wg_conf, std::string("[Interface]\nPrivateKey = k"));
    auto back = parse_freeturn_uri(build_freeturn_uri(*p));
    REQUIRE(back.has_value());
    CHECK_EQ(back->secrets.wg_conf, p->secrets.wg_conf);
}
