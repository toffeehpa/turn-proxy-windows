#include "test.h"

#include "core/connection_state.h"

using namespace ft;
using S = ConnectionState;

TEST_CASE("state: allowed transitions") {
    CHECK(can_transition(S::Disconnected, S::Starting));
    CHECK(can_transition(S::Starting, S::Connecting));
    CHECK(can_transition(S::Connecting, S::Connected));
    CHECK(can_transition(S::Connected, S::Connecting));
    CHECK(can_transition(S::Connected, S::Captcha));
    CHECK(can_transition(S::Captcha, S::Connected));
    CHECK(can_transition(S::Connected, S::Reconnecting));
    CHECK(can_transition(S::Reconnecting, S::Starting));
    CHECK(can_transition(S::Connected, S::Stopping));
    CHECK(can_transition(S::Stopping, S::Disconnected));
    CHECK(can_transition(S::Error, S::Starting));
    CHECK(can_transition(S::Error, S::Disconnected));
}

TEST_CASE("state: forbidden transitions") {
    CHECK(!can_transition(S::Disconnected, S::Connected));
    CHECK(!can_transition(S::Disconnected, S::Stopping));
    CHECK(!can_transition(S::Stopping, S::Connected));
    CHECK(!can_transition(S::Stopping, S::Starting));
    CHECK(!can_transition(S::Error, S::Connected));
    CHECK(!can_transition(S::Reconnecting, S::Connected));
    CHECK(!can_transition(S::Connected, S::Connected));
    CHECK(!can_transition(S::Starting, S::Connected));
}

TEST_CASE("state: every state reaches Disconnected") {
    // Из любого состояния есть путь домой - иначе пользователь застрянет.
    const S all[] = {S::Disconnected, S::Starting,     S::Connecting, S::Connected,
                     S::Captcha,      S::Reconnecting, S::Stopping,   S::Error};
    for (const S from : all) {
        bool reach = from == S::Disconnected || can_transition(from, S::Disconnected) ||
                     (can_transition(from, S::Stopping) && can_transition(S::Stopping, S::Disconnected));
        CHECK(reach);
        CHECK(!to_string(from).empty());
    }
    CHECK(!is_busy(S::Disconnected));
    CHECK(!is_busy(S::Error));
    CHECK(is_busy(S::Connected));
    CHECK(is_busy(S::Reconnecting));
}
