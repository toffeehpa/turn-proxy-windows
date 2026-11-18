#include "test.h"

#include <windows.h>

#include <condition_variable>
#include <mutex>

#include "core/ipc_protocol.h"
#include "core/profile.h"
#include "win/pipe_ipc.h"
#include "win/wstr.h"

using namespace ft;

namespace {

std::wstring unique_pipe() {
    return L"\\\\.\\pipe\\FreeTurn-test-" + win::widen(new_profile_id());
}

struct Inbox {
    std::mutex mu;
    std::condition_variable cv;
    std::vector<std::string> messages;

    void push(std::string m) {
        std::lock_guard lock(mu);
        messages.push_back(std::move(m));
        cv.notify_all();
    }
    bool wait_for(std::size_t n) {
        std::unique_lock lock(mu);
        return cv.wait_for(lock, std::chrono::seconds(5), [&] { return messages.size() >= n; });
    }
};

} // namespace

TEST_CASE("pipe: messages reach the first instance") {
    const auto name = unique_pipe();
    Inbox inbox;
    win::PipeServer server;
    REQUIRE(server.start(name, [&](std::string m) { inbox.push(std::move(m)); }));

    const auto msg = encode_ipc({IpcCommand::Kind::Import, "freeturn://abc"});
    CHECK(win::send_to_pipe(name, msg) == win::PipeSendResult::Sent);
    CHECK(win::send_to_pipe(name, encode_ipc({})) == win::PipeSendResult::Sent);
    REQUIRE(inbox.wait_for(2));
    CHECK_EQ(inbox.messages[0], msg);
    server.stop();
}

TEST_CASE("pipe: name cannot be taken twice") {
    const auto name = unique_pipe();
    win::PipeServer a;
    win::PipeServer b;
    REQUIRE(a.start(name, [](std::string) {}));
    CHECK(!b.start(name, [](std::string) {}));
}

TEST_CASE("pipe: no server and oversize message") {
    const auto name = unique_pipe();
    CHECK(win::send_to_pipe(name, "x", 200) == win::PipeSendResult::NotRunning);
    win::PipeServer server;
    REQUIRE(server.start(name, [](std::string) {}));
    CHECK(win::send_to_pipe(name, std::string(kMaxIpcMessage + 1, 'a')) == win::PipeSendResult::Failed);
}

TEST_CASE("pipe: stop while idle returns promptly") {
    win::PipeServer server;
    REQUIRE(server.start(unique_pipe(), [](std::string) {}));
    const auto t0 = GetTickCount64();
    server.stop();
    CHECK(GetTickCount64() - t0 < 2000);
}
