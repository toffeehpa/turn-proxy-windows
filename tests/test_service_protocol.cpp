#include "test.h"

#include "core/core_args.h"
#include <algorithm>

#include "core/service_protocol.h"
#include "helpers.h"

using namespace ft;

TEST_CASE("service: start request carries profile with secrets") {
    const auto p = test::vk_profile();
    auto r = decode_request(encode_request({ServiceRequest::Kind::Start, p}));
    REQUIRE(r.has_value());
    CHECK(r->kind == ServiceRequest::Kind::Start);
    auto expected = p;
    expected.id = r->profile.id;
    CHECK(r->profile == expected);
    CHECK(decode_request(encode_request({ServiceRequest::Kind::Stop, {}}))->kind ==
          ServiceRequest::Kind::Stop);
}

TEST_CASE("service: invalid or foreign requests are rejected") {
    auto bad = test::vk_profile();
    bad.peer = "host:80 -turn evil";
    CHECK(!decode_request(encode_request({ServiceRequest::Kind::Start, bad})).has_value());
    CHECK(!decode_request(R"({"op":"exec","cmd":"calc"})").has_value());
    CHECK(!decode_request(R"({"op":"start"})").has_value());
    CHECK(!decode_request("not json").has_value());
    CHECK(!decode_request(std::string(kMaxServiceMessage + 1, ' ')).has_value());
}

TEST_CASE("service: events roundtrip") {
    const ServiceEvent events[] = {
        {ServiceEvent::Kind::Output, "2026/11/03 09:15:00 [INFO] line \"q\"", 0},
        {ServiceEvent::Kind::Exited, "", 3},
        {ServiceEvent::Kind::Error, "busy", 0},
    };
    for (const auto& e : events) {
        auto back = decode_event(encode_event(e));
        REQUIRE(back.has_value());
        CHECK(*back == e);
    }
    CHECK(!decode_event(R"({"ev":"boom"})").has_value());
}

TEST_CASE("args: routes only on request") {
    const auto p = test::vk_profile();
    const auto plain = build_core_args(p);
    CHECK(std::ranges::find(plain, "-routes") == plain.end());
    const auto with = build_core_args(p, {.debug = true, .routes = true});
    CHECK(std::ranges::find(with, "-routes") != with.end());
}
