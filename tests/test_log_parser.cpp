#include "test.h"

#include "core/log_parser.h"

using namespace ft;

TEST_CASE("log: split timestamp and level") {
    auto l = split_core_log_line("2026/11/03 09:15:00 [WARN] something\r\n");
    CHECK(l.level == LogLevel::Warn);
    CHECK_EQ(l.text, std::string_view("something"));
    l = split_core_log_line("no timestamp here");
    CHECK(l.level == LogLevel::None);
    CHECK_EQ(l.text, std::string_view("no timestamp here"));
}

TEST_CASE("log: udp streams from debug lines") {
    CoreLogParser p;
    CHECK(p.feed("2026/11/03 09:15:00 [INFO] lifecycle start mode=udp streams=12"));
    CHECK_EQ(p.status().total, 12);
    CHECK(p.feed("2026/11/03 09:15:01 [DEBUG] [STREAM 0] Established DTLS connection"));
    CHECK(p.feed("2026/11/03 09:15:01 [DEBUG] [STREAM 3] Established DTLS connection"));
    // Повтор того же потока не должен раздувать счётчик.
    CHECK(!p.feed("2026/11/03 09:15:01 [DEBUG] [STREAM 3] Established DTLS connection"));
    CHECK_EQ(p.status().active, 2);
    CHECK(p.feed("2026/11/03 09:15:09 [DEBUG] [STREAM 0] Closed DTLS connection"));
    CHECK_EQ(p.status().active, 1);
    CHECK(p.feed("2026/11/03 09:16:00 [INFO] lifecycle stop elapsed=1m error=<nil>"));
    CHECK_EQ(p.status().active, 0);
}

TEST_CASE("log: tcp pool lines") {
    CoreLogParser p;
    p.feed("2026/11/03 09:15:00 [INFO] TCP mode: listening on 127.0.0.1:9000 (sessions=4 bond=false)");
    CHECK(p.feed("2026/11/03 09:15:02 [INFO] [session 1] connected (active: 1)"));
    CHECK(p.feed("2026/11/03 09:15:02 [INFO] TCP mode: pool serving traffic (active: 3)"));
    CHECK_EQ(p.status().active, 3);
    CHECK(p.feed("2026/11/03 09:15:05 [INFO] [session 2] disconnected (active: 2), reconnecting..."));
    CHECK_EQ(p.status().active, 2);
}

TEST_CASE("log: captcha and fatal") {
    CoreLogParser p;
    CHECK(p.feed("2026/11/03 09:15:00 [INFO] [STREAM 0] [Captcha] Triggering manual captcha fallback"));
    CHECK(p.status().captcha);
    CHECK(p.feed("2026/11/03 09:15:30 [INFO] [STREAM 0] [Captcha] Got token from browser"));
    CHECK(!p.status().captcha);
    p.feed("2026/11/03 09:15:00 [INFO] [STREAM 0] [Captcha] Triggering manual captcha fallback");
    p.feed("2026/11/03 09:15:01 [DEBUG] [STREAM 0] Established DTLS connection");
    CHECK(!p.status().captcha);
    CHECK(p.feed("2026/11/03 09:20:00 [ERROR] [STREAM 0] [Captcha] FATAL: 0 connected streams and manual "
                 "captcha failed/timed out"));
    CHECK(p.status().fatal);
    CHECK(p.status().last_error.starts_with("[STREAM 0] [Captcha] FATAL"));
}

TEST_CASE("log: config errors without level") {
    CoreLogParser p;
    p.feed("2026/11/03 09:15:00 invalid -mode value \"x\": must be udp | tcp");
    CHECK(p.status().last_error.starts_with("invalid -mode value"));
    CHECK(!p.status().fatal);
    p.feed("2026/11/03 09:15:00 [ERROR] fatal: udprelay: fatal error");
    CHECK(p.status().fatal);
    p.reset();
    CHECK(p.status() == EngineStatus{});
}

TEST_CASE("log: long errors are clipped") {
    CoreLogParser p;
    p.feed("2026/11/03 09:15:00 [ERROR] " + std::string(1000, 'x'));
    CHECK(p.status().last_error.size() < 400);
}

TEST_CASE("log: redactor") {
    SecretRedactor r;
    r.add("abc");
    r.add("supersecretkey");
    r.add("https://example.invalid/call/join/supersecretkey");
    const auto out = r.apply("link https://example.invalid/call/join/supersecretkey key supersecretkey abc");
    CHECK_EQ(out, std::string("link *** key *** abc"));
}
