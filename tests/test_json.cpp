#include "test.h"

#include "core/json.h"

using namespace ft;

TEST_CASE("json: parse primitives and nesting") {
    auto v = json::parse(R"({"a":1,"b":[true,false,null],"c":{"d":"x\ny"},"e":-2.5e1})");
    REQUIRE(v.has_value());
    CHECK_EQ(v->find("a")->as_int(), 1);
    CHECK_EQ(v->find("b")->as_array().size(), std::size_t{3});
    CHECK(v->find("b")->as_array()[2].is_null());
    CHECK_EQ(v->find("c")->find("d")->as_string(), std::string("x\ny"));
    CHECK_EQ(v->find("e")->as_double(), -25.0);
    CHECK(v->find("zzz") == nullptr);
}

TEST_CASE("json: unicode escapes and surrogates") {
    auto v = json::parse(R"(["Ж", "😀", "\/"])");
    REQUIRE(v.has_value());
    CHECK_EQ(v->as_array()[0].as_string(), std::string("\xD0\x96"));
    CHECK_EQ(v->as_array()[1].as_string(), std::string("\xF0\x9F\x98\x80"));
    CHECK_EQ(v->as_array()[2].as_string(), std::string("/"));
    CHECK(!json::parse(R"(["\ud83d"])").has_value());
    CHECK(!json::parse(R"(["\ude00"])").has_value());
}

TEST_CASE("json: rejects garbage") {
    CHECK(!json::parse("").has_value());
    CHECK(!json::parse("{").has_value());
    CHECK(!json::parse("{\"a\":}").has_value());
    CHECK(!json::parse("[1,]").has_value());
    CHECK(!json::parse("01").has_value());
    CHECK(!json::parse("1 2").has_value());
    CHECK(!json::parse("\"a\x01\"").has_value());
    CHECK(!json::parse("tru").has_value());
    CHECK(!json::parse("1e999").has_value());
}

TEST_CASE("json: depth limit") {
    std::string deep(json::kMaxDepth + 5, '[');
    deep += std::string(json::kMaxDepth + 5, ']');
    CHECK(!json::parse(deep).has_value());
    std::string ok(10, '[');
    ok += std::string(10, ']');
    CHECK(json::parse(ok).has_value());
}

TEST_CASE("json: BOM and duplicate keys") {
    auto v = json::parse("\xEF\xBB\xBF{\"a\":1,\"a\":2}");
    REQUIRE(v.has_value());
    CHECK_EQ(v->as_object().size(), std::size_t{1});
    CHECK_EQ(v->find("a")->as_int(), 2);
}

TEST_CASE("json: serialize roundtrip keeps order") {
    json::Value o = json::Object{{"z", 1}, {"a", "q\"\\\x01"}, {"m", json::Array{1.5, true, nullptr}}};
    const auto compact = json::serialize(o, -1);
    CHECK_EQ(compact, std::string(R"({"z":1,"a":"q\"\\\u0001","m":[1.5,true,null]})"));
    auto back = json::parse(json::serialize(o));
    REQUIRE(back.has_value());
    CHECK_EQ(json::serialize(*back, -1), compact);
}

TEST_CASE("json: values copy deeply") {
    json::Value a = json::Object{{"k", 1}};
    json::Value b = a;
    b.set("k", 2);
    CHECK_EQ(a.find("k")->as_int(), 1);
    CHECK_EQ(b.find("k")->as_int(), 2);
}

TEST_CASE("json: as_int is strict") {
    CHECK_EQ(json::Value(1.5).as_int(7), 7);
    CHECK_EQ(json::Value(1e12).as_int(7), 7);
    CHECK_EQ(json::Value("3").as_int(7), 7);
    CHECK_EQ(json::Value(-3).as_int(7), -3);
}
