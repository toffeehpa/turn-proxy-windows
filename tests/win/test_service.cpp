#include "test.h"

#include "helpers.h"
#include "win/events.h"
#include "win/service_engine.h"
#include "win/service_host.h"
#include "win/single_instance.h"
#include "win/wstr.h"

using namespace ft;

namespace {

std::wstring unique_pipe() {
    return L"\\\\.\\pipe\\FreeTurnService-test-" + win::widen(new_profile_id());
}

// Без SCM и SYSTEM: пайп доступен текущему пользователю, ядро подставное, маршруты не трогаем.
win::ServiceHostOptions host_options(const std::wstring& pipe) {
    win::ServiceHostOptions o;
    o.pipe_name = pipe;
    o.sddl = L"D:P(A;;GA;;;" + win::current_user_sid() + L")";
    o.core_path = FAKE_CORE_PATH;
    o.work_dir = std::filesystem::temp_directory_path();
    o.routes = false;
    return o;
}

EngineLaunch launch(Profile p) {
    EngineLaunch l;
    l.profile = std::move(p);
    return l;
}

Profile direct_profile() {
    Profile p;
    p.name = "Direct";
    p.provider = "direct";
    p.peer = "198.51.100.10:56000";
    return p;
}

} // namespace

TEST_CASE("service: start, output and stop through the pipe") {
    const auto pipe = unique_pipe();
    win::ServiceHost host(host_options(pipe));
    REQUIRE(host.start());
    test::Events ev;
    win::ServiceEngine engine(pipe, win::current_user_sid(), [&](EngineEvent e) { ev.push(std::move(e)); });
    REQUIRE(engine.start(5, launch(direct_profile())).has_value());
    REQUIRE(ev.has_line("Established DTLS connection"));
    engine.stop();
    auto code = ev.exit_code();
    REQUIRE(code.has_value());
    CHECK_EQ(*code, 0);
}

TEST_CASE("service: closed client session stops the core") {
    const auto pipe = unique_pipe();
    win::ServiceHost host(host_options(pipe));
    REQUIRE(host.start());
    {
        test::Events ev;
        win::ServiceEngine first(pipe, win::current_user_sid(),
                                 [&](EngineEvent e) { ev.push(std::move(e)); });
        REQUIRE(first.start(1, launch(direct_profile())).has_value());
        REQUIRE(ev.has_line("lifecycle start"));
    }
    // Ядро первого клиента погашено - второй не получает "already running".
    test::Events ev;
    win::ServiceEngine second(pipe, win::current_user_sid(), [&](EngineEvent e) { ev.push(std::move(e)); });
    REQUIRE(second.start(2, launch(direct_profile())).has_value());
    REQUIRE(ev.has_line("lifecycle start"));
    second.stop();
    CHECK(ev.exit_code().has_value());
}

TEST_CASE("service: invalid profile is refused by the service") {
    const auto pipe = unique_pipe();
    win::ServiceHost host(host_options(pipe));
    REQUIRE(host.start());
    test::Events ev;
    win::ServiceEngine engine(pipe, win::current_user_sid(), [&](EngineEvent e) { ev.push(std::move(e)); });
    auto bad = direct_profile();
    bad.peer = "198.51.100.10:56000 -turn 203.0.113.1";
    REQUIRE(engine.start(1, launch(bad)).has_value());
    auto code = ev.exit_code();
    REQUIRE(code.has_value());
    CHECK_EQ(*code, -1);
    CHECK(ev.has_line("[ERROR] invalid profile"));
}

TEST_CASE("service: pipe of an unexpected owner gets no profile") {
    const auto pipe = unique_pipe();
    win::ServiceHost host(host_options(pipe));
    REQUIRE(host.start());
    win::ServiceEngine engine(pipe, L"S-1-5-18", [](EngineEvent) {});
    auto r = engine.start(1, launch(test::vk_profile()));
    REQUIRE(!r.has_value());
    CHECK(r.error().find("untrusted") != std::string::npos);
}

TEST_CASE("service: no service - clear error") {
    win::ServiceEngine engine(unique_pipe(), win::current_user_sid(), [](EngineEvent) {});
    auto r = engine.start(1, launch(direct_profile()));
    REQUIRE(!r.has_value());
    CHECK(r.error().find("not running") != std::string::npos);
}
