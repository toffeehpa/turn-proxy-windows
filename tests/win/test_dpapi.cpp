#include "test.h"

#include "win/dpapi.h"

using namespace ft;

TEST_CASE("dpapi: roundtrip and no plaintext in blob") {
    win::DpapiProtector p;
    const std::string secret = "obf-key-0123456789abcdef-secret";
    auto blob = p.protect(secret);
    REQUIRE(blob.has_value());
    CHECK(blob->find(secret) == std::string::npos);
    auto back = p.unprotect(*blob);
    REQUIRE(back.has_value());
    CHECK_EQ(*back, secret);
}

TEST_CASE("dpapi: tampered blob is rejected") {
    win::DpapiProtector p;
    auto blob = p.protect("payload");
    REQUIRE(blob.has_value());
    (*blob)[blob->size() / 2] = static_cast<char>((*blob)[blob->size() / 2] ^ 0x01);
    CHECK(!p.unprotect(*blob).has_value());
    CHECK(!p.unprotect("not a blob").has_value());
}
