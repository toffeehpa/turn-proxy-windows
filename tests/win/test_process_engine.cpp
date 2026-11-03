#include "test.h"

#include <windows.h>

#include "win/events.h"
#include "win/process_engine.h"

using namespace ft;

namespace {

std::filesystem::path fake_core() {
    return FAKE_CORE_PATH;
}

EngineLaunch launch(std::vector<std::string> args) {
    EngineLaunch l;
    l.args = std::move(args);
    l.display_args = l.args;
    return l;
}

} // namespace

TEST_CASE("engine: graceful stop via Ctrl+Break") {
    test::Events ev;
    win::ProcessEngine engine(fake_core, std::filesystem::temp_directory_path(),
                              [&](EngineEvent e) { ev.push(std::move(e)); });
    REQUIRE(engine.start(7, launch({"-fake", "ok"})).has_value());
    REQUIRE(ev.has_line("Established DTLS connection"));
    CHECK(!engine.start(8, launch({"-fake", "ok"})).has_value());
    engine.stop();
    auto code = ev.exit_code();
    REQUIRE(code.has_value());
    CHECK_EQ(*code, 0);
    CHECK(ev.has_line("Terminating..."));
    CHECK(ev.list.front().session == 7u);
}

TEST_CASE("engine: stuck core is killed via job") {
    test::Events ev;
    win::ProcessEngine engine(fake_core, std::filesystem::temp_directory_path(),
                              [&](EngineEvent e) { ev.push(std::move(e)); });
    REQUIRE(engine.start(1, launch({"-fake", "ignore"})).has_value());
    REQUIRE(ev.has_line("lifecycle start"));
    const auto t0 = GetTickCount64();
    engine.shutdown(500);
    auto code = ev.exit_code();
    REQUIRE(code.has_value());
    CHECK_EQ(*code, 1);
    CHECK(GetTickCount64() - t0 < 5000);
}

TEST_CASE("engine: exit code and error lines") {
    test::Events ev;
    win::ProcessEngine engine(fake_core, {}, [&](EngineEvent e) { ev.push(std::move(e)); });
    REQUIRE(engine.start(1, launch({"-fake", "fail"})).has_value());
    auto code = ev.exit_code();
    REQUIRE(code.has_value());
    CHECK_EQ(*code, 3);
    CHECK(ev.has_line("invalid -mode"));
    // Монитор отработал - можно запускать снова.
    REQUIRE(engine.start(2, launch({"-fake", "fail"})).has_value());
    engine.shutdown(1000);
}

TEST_CASE("engine: arguments survive quoting") {
    test::Events ev;
    win::ProcessEngine engine(fake_core, {}, [&](EngineEvent e) { ev.push(std::move(e)); });
    REQUIRE(engine.start(1, launch({"-fake", "echo", "with space", R"(tail\)", "q\"uote", ""})).has_value());
    REQUIRE(ev.exit_code().has_value());
    CHECK(ev.has_line("ARG with space"));
    CHECK(ev.has_line(R"(ARG tail\)"));
    CHECK(ev.has_line("ARG q\"uote"));
}

TEST_CASE("engine: missing core is an error, not a fake connect") {
    win::ProcessEngine engine([] { return std::filesystem::path(L"C:\\nope\\freeturn-core.exe"); }, {},
                              [](EngineEvent) {});
    auto r = engine.start(1, launch({}));
    REQUIRE(!r.has_value());
    CHECK(r.error().find("freeturn-core.exe not found") != std::string::npos);
}
