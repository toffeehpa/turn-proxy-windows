#include "test.h"

#include <algorithm>

#include "core/core_args.h"
#include "helpers.h"

using namespace ft;

namespace {
using Args = std::vector<std::string>;

bool contains_seq(const Args& args, const Args& seq) {
    return std::ranges::search(args, seq).begin() != args.end();
}
} // namespace

TEST_CASE("args: vk defaults are omitted") {
    const auto args = build_core_args(test::vk_profile());
    CHECK(contains_seq(args, {"-peer", "203.0.113.7:56000"}));
    CHECK(contains_seq(args, {"-links", "https://example.invalid/call/join/AAAA"}));
    CHECK(contains_seq(args, {"-obf-profile", "rtpopus3", "-obf-key", test::kKey}));
    CHECK(contains_seq(args, {"-client-id", "a1b2c3d4e5f60718293a4b5c6d7e8f90"}));
    CHECK_EQ(args.back(), std::string("-debug"));
    for (const char* absent :
         {"-provider", "-listen", "-n", "-mode", "-transport", "-dns-mode", "-platform"}) {
        CHECK(std::ranges::find(args, absent) == args.end());
    }
}

TEST_CASE("args: non-default values") {
    auto p = test::vk_profile();
    p.provider = "direct";
    p.listen = "127.0.0.1:9100";
    p.streams = 4;
    p.transport = "udp";
    p.mode = "tcp";
    p.bond = true;
    p.kcp.interval = 40;
    p.kcp.ack_no_delay = false;
    p.obf_timing_ms = 20;
    p.dns_mode = "doh";
    p.dns_servers = {"1.1.1.1", "8.8.8.8"};
    const auto args = build_core_args(p, {.debug = false});
    CHECK(contains_seq(args, {"-provider", "direct"}));
    CHECK(std::ranges::find(args, "-links") == args.end());
    CHECK(contains_seq(args, {"-listen", "127.0.0.1:9100"}));
    CHECK(contains_seq(args, {"-n", "4"}));
    CHECK(contains_seq(args, {"-transport", "udp"}));
    CHECK(contains_seq(args, {"-mode", "tcp", "-bond"}));
    CHECK(contains_seq(args, {"-kcp-interval", "40"}));
    CHECK(std::ranges::find(args, "-kcp-acknodelay=false") != args.end());
    CHECK(std::ranges::find(args, "-kcp-mtu") == args.end());
    CHECK(contains_seq(args, {"-obf-timing", "20ms"}));
    CHECK(contains_seq(args, {"-dns-mode", "doh", "-dns-servers", "1.1.1.1,8.8.8.8"}));
    CHECK(std::ranges::find(args, "-debug") == args.end());
}

TEST_CASE("args: redaction hides secrets") {
    auto args = build_core_args(test::vk_profile());
    args.push_back("-obf-key=" + test::kKey);
    args.push_back("freeturn://abc");
    const auto shown = redact_core_args(args);
    const auto line = join_windows_command_line(shown);
    CHECK(line.find(test::kKey) == std::string::npos);
    CHECK(line.find("call/join") == std::string::npos);
    CHECK(line.find("a1b2c3d4") == std::string::npos);
    CHECK(line.find("freeturn://abc") == std::string::npos);
    CHECK(line.find("203.0.113.7:56000") != std::string::npos);
    CHECK_EQ(shown.size(), args.size());
}

TEST_CASE("args: windows quoting roundtrip") {
    const Args cases[] = {
        {"plain", "with space", ""},
        {R"(C:\Program Files\FreeTurn\)", R"(a\"b)", R"(tail\\)"},
        {"tab\there", "quote\"inside", R"(\\server\share)"},
    };
    for (const auto& c : cases) {
        CHECK(split_windows_command_line(join_windows_command_line(c)) == c);
    }
    CHECK_EQ(quote_windows_arg("abc"), std::string("abc"));
    CHECK_EQ(quote_windows_arg(""), std::string("\"\""));
    CHECK_EQ(quote_windows_arg(R"(a b\)"), std::string(R"("a b\\")"));
}

TEST_CASE("args: wg config turns on built-in tunnel") {
    auto p = test::vk_profile();
    p.secrets.wg_conf = "[Interface]\nPrivateKey = secret";
    const auto args = build_core_args(p, {.debug = true, .routes = true});
    CHECK(std::ranges::find(args, "-tun") != args.end());
    CHECK(contains_seq(args, {"-tun-conf", p.secrets.wg_conf}));
    CHECK(std::ranges::find(args, "-routes") == args.end());
    CHECK(join_windows_command_line(redact_core_args(args)).find("secret") == std::string::npos);

    p.mode = std::string(kModeTcp);
    const auto tcp = build_core_args(p);
    CHECK(std::ranges::find(tcp, "-tun") == tcp.end());
}
