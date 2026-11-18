#include "test.h"

#include "core/ipc_protocol.h"

using namespace ft;
using K = IpcCommand::Kind;

TEST_CASE("ipc: encode/decode roundtrip") {
    const IpcCommand cmds[] = {
        {K::Show, ""},
        {K::Import, "freeturn://eyJ2IjoxfQ"},
        {K::Connect, "Home (2)"},
        {K::Connect, ""},
        {K::Disconnect, ""},
        {K::Reconnect, ""},
        {K::Quit, ""},
    };
    for (const auto& c : cmds) {
        auto back = decode_ipc(encode_ipc(c));
        REQUIRE(back.has_value());
        CHECK(*back == c);
    }
}

TEST_CASE("ipc: rejects junk") {
    CHECK(!decode_ipc("").has_value());
    CHECK(!decode_ipc("show").has_value());
    CHECK(!decode_ipc("FTIPC1 rm -rf").has_value());
    CHECK(!decode_ipc("FTIPC1 import http://evil").has_value());
    CHECK(!decode_ipc("FTIPC1 show extra").has_value());
    CHECK(!decode_ipc("FTIPC1 connect a\nb").has_value());
    CHECK(!decode_ipc("FTIPC1 connect " + std::string(kMaxIpcMessage, 'a')).has_value());
}

TEST_CASE("ipc: command line of second instance") {
    CHECK(command_from_args({}) == IpcCommand{});
    CHECK(command_from_args({"freeturn://abc"}) == (IpcCommand{K::Import, "freeturn://abc"}));
    CHECK(command_from_args({"--import", "freeturn://abc"}) == (IpcCommand{K::Import, "freeturn://abc"}));
    CHECK(command_from_args({"--connect", "Home"}) == (IpcCommand{K::Connect, "Home"}));
    CHECK(command_from_args({"--disconnect"}) == (IpcCommand{K::Disconnect, ""}));
    CHECK(command_from_args({"--whatever"}) == IpcCommand{});
}
