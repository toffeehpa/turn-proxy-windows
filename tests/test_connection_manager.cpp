#include "test.h"

#include <deque>

#include "core/connection_manager.h"
#include "helpers.h"

using namespace ft;
using S = ConnectionState;

namespace {

// Mock движка: только для тестов, в релизе фиктивного "подключено" нет.
class MockEngine : public IEngine {
public:
    int starts = 0;
    int stops = 0;
    bool fail_start = false;
    SessionId session = 0;
    EngineLaunch last;

    std::expected<void, std::string> start(SessionId s, const EngineLaunch& launch) override {
        ++starts;
        if (fail_start) {
            return std::unexpected("freeturn-core.exe not found");
        }
        session = s;
        last = launch;
        return {};
    }
    void stop() override { ++stops; }
};

class MockTimer : public IReconnectTimer {
public:
    std::optional<std::uint32_t> scheduled;
    int cancels = 0;
    void schedule(std::uint32_t ms) override { scheduled = ms; }
    void cancel() override {
        ++cancels;
        scheduled.reset();
    }
};

struct Fixture {
    MockEngine engine;
    MockTimer timer;
    ConnectionManager cm{engine, timer};
    std::vector<S> states;
    std::vector<std::string> logs;

    Fixture() {
        cm.on_change([this](const ConnectionSnapshot& s) {
            if (states.empty() || states.back() != s.state) {
                states.push_back(s.state);
            }
        });
        cm.on_log([this](LogLevel, std::string_view t) { logs.emplace_back(t); });
    }

    void line(std::string_view text) {
        cm.on_engine_event({EngineEvent::Kind::Output, engine.session, std::string(text), 0});
    }
    void exited(int code) { cm.on_engine_event({EngineEvent::Kind::Exited, engine.session, {}, code}); }
    void up() { line("2026/11/03 09:15:01 [DEBUG] [STREAM 0] Established DTLS connection"); }
};

} // namespace

TEST_CASE("cm: connect, connected, disconnect") {
    Fixture f;
    CHECK(f.cm.connect(test::vk_profile()) == ConnectResult::Started);
    CHECK(f.cm.state() == S::Connecting);
    CHECK_EQ(f.engine.starts, 1);
    CHECK_EQ(f.cm.snapshot().local_address, std::string("127.0.0.1:9000"));
    CHECK_EQ(f.cm.snapshot().total, 12);

    f.up();
    CHECK(f.cm.state() == S::Connected);
    CHECK_EQ(f.cm.snapshot().active, 1);

    f.cm.disconnect();
    CHECK(f.cm.state() == S::Stopping);
    CHECK_EQ(f.engine.stops, 1);
    f.exited(1);
    CHECK(f.cm.state() == S::Disconnected);
    CHECK(f.states ==
          (std::vector<S>{S::Starting, S::Connecting, S::Connected, S::Stopping, S::Disconnected}));
}

TEST_CASE("cm: invalid and experimental profiles do not start") {
    Fixture f;
    auto bad = test::vk_profile();
    bad.peer.clear();
    CHECK(f.cm.connect(bad) == ConnectResult::InvalidProfile);
    auto sfu = test::vk_profile();
    sfu.provider = "sfu";
    CHECK(f.cm.connect(sfu) == ConnectResult::ExperimentalDisabled);
    CHECK_EQ(f.engine.starts, 0);
    f.cm.set_options({.allow_sfu = true});
    CHECK(f.cm.connect(sfu) == ConnectResult::Started);
}

TEST_CASE("cm: launch failure goes to Error") {
    Fixture f;
    f.engine.fail_start = true;
    CHECK(f.cm.connect(test::vk_profile()) == ConnectResult::LaunchFailed);
    CHECK(f.cm.state() == S::Error);
    CHECK_EQ(f.cm.snapshot().error, std::string("freeturn-core.exe not found"));
    f.cm.disconnect();
    CHECK(f.cm.state() == S::Disconnected);
    CHECK(f.cm.snapshot().error.empty());
}

TEST_CASE("cm: config error before connect - no auto reconnect") {
    Fixture f;
    f.cm.connect(test::vk_profile());
    f.line("2026/11/03 09:15:00 invalid -mode value \"x\": must be udp | tcp");
    f.exited(1);
    CHECK(f.cm.state() == S::Error);
    CHECK(!f.timer.scheduled.has_value());
    CHECK(f.cm.snapshot().error.starts_with("invalid -mode"));
}

TEST_CASE("cm: crash after connect - backoff reconnect") {
    Fixture f;
    f.cm.connect(test::vk_profile());
    f.up();
    f.exited(2);
    CHECK(f.cm.state() == S::Reconnecting);
    REQUIRE(f.timer.scheduled.has_value());
    CHECK_EQ(*f.timer.scheduled, 2000u);
    CHECK_EQ(f.cm.snapshot().attempt, 1);

    f.cm.on_reconnect_timer();
    CHECK(f.cm.state() == S::Connecting);
    CHECK_EQ(f.engine.starts, 2);
    f.exited(2);
    CHECK(f.cm.state() == S::Reconnecting);
    CHECK_EQ(*f.timer.scheduled, 4000u);

    f.cm.on_reconnect_timer();
    f.up();
    CHECK(f.cm.state() == S::Connected);
    CHECK_EQ(f.cm.snapshot().attempt, 0);
}

TEST_CASE("cm: reconnect gives up after max attempts") {
    Fixture f;
    f.cm.connect(test::vk_profile());
    f.up();
    for (int i = 0; i < kMaxReconnectAttempts; ++i) {
        f.exited(1);
        CHECK(f.cm.state() == S::Reconnecting);
        f.cm.on_reconnect_timer();
    }
    f.exited(1);
    CHECK(f.cm.state() == S::Error);
}

TEST_CASE("cm: fatal captcha is not retried") {
    Fixture f;
    f.cm.connect(test::vk_profile());
    f.up();
    f.line("2026/11/03 09:15:00 [INFO] [STREAM 1] [Captcha] Triggering manual captcha fallback");
    CHECK(f.cm.state() == S::Captcha);
    f.line("2026/11/03 09:20:00 [ERROR] [STREAM 1] [Captcha] FATAL: 0 connected streams and manual captcha "
           "failed/timed out");
    f.exited(1);
    CHECK(f.cm.state() == S::Error);
}

TEST_CASE("cm: disconnect during backoff cancels timer") {
    Fixture f;
    f.cm.connect(test::vk_profile());
    f.up();
    f.exited(1);
    REQUIRE(f.timer.scheduled.has_value());
    f.cm.disconnect();
    CHECK(f.cm.state() == S::Disconnected);
    CHECK(!f.timer.scheduled.has_value());
    f.cm.on_reconnect_timer();
    CHECK(f.cm.state() == S::Disconnected);
    CHECK_EQ(f.engine.starts, 1);
}

TEST_CASE("cm: switch profile stops then starts the other") {
    Fixture f;
    f.cm.connect(test::vk_profile("A"));
    f.up();
    auto b = test::vk_profile("B");
    b.id = "fedcba9876543210fedcba9876543210";
    CHECK(f.cm.connect(b) == ConnectResult::Switching);
    CHECK(f.cm.state() == S::Stopping);
    const auto old = f.engine.session;
    f.exited(0);
    CHECK(f.cm.state() == S::Connecting);
    CHECK(f.engine.session != old);
    CHECK_EQ(f.cm.snapshot().profile_name, std::string("B"));
    CHECK(f.cm.connect(b) == ConnectResult::AlreadyActive);
}

TEST_CASE("cm: manual reconnect restarts same profile") {
    Fixture f;
    f.cm.connect(test::vk_profile());
    f.up();
    f.cm.reconnect();
    CHECK(f.cm.state() == S::Stopping);
    f.exited(0);
    CHECK(f.cm.state() == S::Connecting);
    CHECK_EQ(f.engine.starts, 2);
}

TEST_CASE("cm: stale events from previous session are ignored") {
    Fixture f;
    f.cm.connect(test::vk_profile());
    const auto first = f.engine.session;
    f.cm.reconnect();
    f.exited(0);
    REQUIRE(f.engine.session != first);
    f.cm.on_engine_event({EngineEvent::Kind::Exited, first, {}, 1});
    f.cm.on_engine_event({EngineEvent::Kind::Output, first,
                          "2026/11/03 09:15:01 [DEBUG] [STREAM 0] Established DTLS connection", 0});
    CHECK(f.cm.state() == S::Connecting);
}

TEST_CASE("cm: secrets never reach the log") {
    Fixture f;
    f.cm.set_options({.verbose_core_log = true});
    f.cm.connect(test::vk_profile());
    f.line("2026/11/03 09:15:00 [INFO] Client ID: a1b2c3d4e5f60718293a4b5c6d7e8f90");
    f.line("2026/11/03 09:15:00 [DEBUG] link https://example.invalid/call/join/AAAA key " + test::kKey);
    for (const auto& l : f.logs) {
        CHECK(l.find(test::kKey) == std::string::npos);
        CHECK(l.find("a1b2c3d4e5f6") == std::string::npos);
        CHECK(l.find("call/join/AAAA") == std::string::npos);
    }
    CHECK(f.engine.last.args != f.engine.last.display_args);
}

TEST_CASE("cm: debug core lines hidden by default") {
    Fixture f;
    f.cm.connect(test::vk_profile());
    const auto before = f.logs.size();
    f.up();
    for (std::size_t i = before; i < f.logs.size(); ++i) {
        CHECK(f.logs[i].find("Established DTLS") == std::string::npos);
    }
}

TEST_CASE("cm: backoff schedule") {
    CHECK_EQ(reconnect_delay_ms(1), 2000u);
    CHECK_EQ(reconnect_delay_ms(4), 16000u);
    CHECK_EQ(reconnect_delay_ms(9), 30000u);
}
