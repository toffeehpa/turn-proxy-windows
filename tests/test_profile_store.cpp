#include "test.h"

#include <filesystem>

#include "core/profile_store.h"
#include "helpers.h"

using namespace ft;
namespace fs = std::filesystem;

namespace {

// Подделка DPAPI: обратимый XOR с маркером, чтобы ловить "забыли расшифровать".
class FakeProtector : public ISecretProtector {
public:
    bool fail = false;
    std::optional<std::string> protect(std::string_view plain) override {
        if (fail) {
            return std::nullopt;
        }
        std::string out = "FAKE";
        for (const char c : plain) {
            out.push_back(static_cast<char>(c ^ 0x5A));
        }
        return out;
    }
    std::optional<std::string> unprotect(std::string_view blob) override {
        if (!blob.starts_with("FAKE")) {
            return std::nullopt;
        }
        std::string out;
        for (const char c : blob.substr(4)) {
            out.push_back(static_cast<char>(c ^ 0x5A));
        }
        return out;
    }
};

struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() / ("freeturn-test-" + new_profile_id());
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

} // namespace

TEST_CASE("store: save, load, secrets stay encrypted") {
    TempDir dir;
    FakeProtector prot;
    ProfileStore store(dir.path, prot);
    auto p = test::vk_profile();
    p.id.clear();
    REQUIRE(store.save(p).has_value());
    CHECK(is_profile_id(p.id));

    const auto pub = read_file(store.profiles_dir() / (p.id + ".json"));
    REQUIRE(pub.has_value());
    CHECK(pub->find(test::kKey) == std::string::npos);
    const auto blob = read_file(store.secure_dir() / (p.id + ".bin"));
    REQUIRE(blob.has_value());
    CHECK(blob->find(test::kKey) == std::string::npos);

    auto loaded = store.load_all();
    REQUIRE(loaded.profiles.size() == 1);
    CHECK(loaded.broken.empty());
    CHECK(loaded.profiles[0] == p);
}

TEST_CASE("store: broken files are reported, not fatal") {
    TempDir dir;
    FakeProtector prot;
    ProfileStore store(dir.path, prot);
    auto good = test::vk_profile("Good");
    good.id.clear();
    REQUIRE(store.save(good).has_value());
    const auto bad_id = new_profile_id();
    REQUIRE(write_file_atomic(store.profiles_dir() / (bad_id + ".json"), "{not json").has_value());
    const auto bad_sec = new_profile_id();
    REQUIRE(write_file_atomic(store.profiles_dir() / (bad_sec + ".json"), R"({"name":"X"})").has_value());
    REQUIRE(write_file_atomic(store.secure_dir() / (bad_sec + ".bin"), "garbage").has_value());
    // Чужие файлы с неправильным именем просто пропускаем.
    REQUIRE(write_file_atomic(store.profiles_dir() / "readme.json", "{}").has_value());

    auto loaded = store.load_all();
    CHECK_EQ(loaded.profiles.size(), std::size_t{1});
    CHECK_EQ(loaded.broken.size(), std::size_t{2});
}

TEST_CASE("store: clone and remove") {
    TempDir dir;
    FakeProtector prot;
    ProfileStore store(dir.path, prot);
    auto p = test::vk_profile("Home");
    p.id.clear();
    REQUIRE(store.save(p).has_value());
    auto copy = store.clone(p, {"Home"});
    REQUIRE(copy.has_value());
    CHECK(copy->id != p.id);
    CHECK_EQ(copy->name, std::string("Home (2)"));
    CHECK(copy->secrets == p.secrets);
    CHECK_EQ(store.load_all().profiles.size(), std::size_t{2});

    REQUIRE(store.remove(p.id).has_value());
    CHECK(!fs::exists(store.secure_dir() / (p.id + ".bin")));
    CHECK_EQ(store.load_all().profiles.size(), std::size_t{1});
    CHECK(!store.remove("../settings").has_value());
}

TEST_CASE("store: protector failure keeps old data") {
    TempDir dir;
    FakeProtector prot;
    ProfileStore store(dir.path, prot);
    auto p = test::vk_profile();
    p.id.clear();
    REQUIRE(store.save(p).has_value());
    prot.fail = true;
    auto changed = p;
    changed.peer = "198.51.100.1:1";
    CHECK(!store.save(changed).has_value());
    prot.fail = false;
    const auto loaded = store.load_all();
    REQUIRE(loaded.profiles.size() == 1);
    CHECK_EQ(loaded.profiles[0].peer, p.peer);
}
