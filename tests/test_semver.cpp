#include "test.h"

#include "core/semver.h"

using namespace ft;

namespace {
Version v(std::string_view s) {
    return parse_version(s).value_or(Version{-1, -1, -1, "bad"});
}
} // namespace

TEST_CASE("semver: parse tags") {
    CHECK(v("v0.1.0-beta.1") == (Version{0, 1, 0, "beta.1"}));
    CHECK(v("1.2.3+build.5") == (Version{1, 2, 3, ""}));
    CHECK_EQ(v("v0.1.0-beta.1").str(), std::string("0.1.0-beta.1"));
    CHECK(!parse_version("1.2").has_value());
    CHECK(!parse_version("01.2.3").has_value());
    CHECK(!parse_version("1.2.3-").has_value());
    CHECK(!parse_version("1.2.3-beta..1").has_value());
    CHECK(!parse_version("1.2.3-be ta").has_value());
    CHECK(!parse_version("latest").has_value());
}

TEST_CASE("semver: beta ordering") {
    CHECK(v("0.1.0-beta.1") < v("0.1.0-beta.2"));
    CHECK(v("0.1.0-beta.2") < v("0.1.0-beta.10"));
    CHECK(v("0.1.0-beta.10") < v("0.1.0"));
    CHECK(v("0.1.0") < v("0.1.1-beta.1"));
    CHECK(v("0.1.0-alpha.9") < v("0.1.0-beta.1"));
    CHECK(v("0.1.0-beta") < v("0.1.0-beta.1"));
    CHECK(v("0.1.0-beta.1") < v("0.1.0-beta.rc"));
    CHECK(v("0.9.9") < v("0.10.0"));
    CHECK(v("1.0.0") > v("0.99.99"));
    CHECK(v("v0.1.0") == v("0.1.0"));
}
