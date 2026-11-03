#include "test.h"

#include "core/base64.h"

using namespace ft;

TEST_CASE("base64: url roundtrip all lengths") {
    std::string data;
    for (int i = 0; i < 40; ++i) {
        const auto enc = base64::encode_url(data);
        CHECK(enc.find('=') == std::string::npos);
        auto dec = base64::decode(enc);
        REQUIRE(dec.has_value());
        CHECK(*dec == data);
        data.push_back(static_cast<char>(i * 37 + 250));
    }
}

TEST_CASE("base64: known vectors and std alphabet") {
    CHECK_EQ(base64::encode_url("foobar"), std::string("Zm9vYmFy"));
    CHECK_EQ(base64::encode_url("fo"), std::string("Zm8"));
    CHECK_EQ(base64::decode("Zm8=").value_or(""), std::string("fo"));
    CHECK_EQ(base64::decode("-_8").value_or("a"), base64::decode("+/8=").value_or("b"));
    CHECK(!base64::decode("Z").has_value());
    CHECK(!base64::decode("Zm9v!").has_value());
}
