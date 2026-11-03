#include "core/profile_store.h"

#include <algorithm>
#include <system_error>

#include "core/profile_json.h"

namespace ft {

ProfileStore::ProfileStore(std::filesystem::path root, ISecretProtector& protector)
    : root_(std::move(root)), protector_(protector) {}

StoreLoadResult ProfileStore::load_all() {
    StoreLoadResult result;
    std::error_code ec;
    std::filesystem::directory_iterator it(profiles_dir(), ec);
    if (ec) {
        return result;
    }
    for (const auto& entry : it) {
        const auto& path = entry.path();
        if (path.extension() != ".json" || !entry.is_regular_file(ec)) {
            continue;
        }
        const std::string id = path.stem().string();
        if (!is_profile_id(id)) {
            continue;
        }
        auto text = read_file(path);
        auto v = text ? json::parse(*text) : std::unexpected(json::ParseError{0, "read"});
        auto p = v ? profile_from_json(*v) : std::unexpected(std::string("parse"));
        if (!p) {
            result.broken.push_back(path.filename().string());
            continue;
        }
        p->id = id;
        // Секретов может не быть (профиль direct без ключа) - это не ошибка.
        if (auto blob = read_file(secure_dir() / (id + ".bin"), 1024 * 1024)) {
            auto plain = protector_.unprotect(*blob);
            auto sv = plain ? json::parse(*plain) : std::unexpected(json::ParseError{0, "decrypt"});
            auto secrets = sv ? secrets_from_json(*sv) : std::unexpected(std::string("decrypt"));
            if (!secrets) {
                result.broken.push_back(id + ".bin");
                continue;
            }
            p->secrets = std::move(*secrets);
        }
        result.profiles.push_back(std::move(*p));
    }
    std::ranges::sort(result.profiles, [](const Profile& a, const Profile& b) { return a.name < b.name; });
    return result;
}

std::expected<void, std::string> ProfileStore::save(Profile& p) {
    if (!is_profile_id(p.id)) {
        p.id = new_profile_id();
    }
    p.name = sanitize_name(p.name);
    // Секреты пишем первыми: профиль без своего blob-а хуже, чем blob-сирота.
    const auto secrets = json::serialize(secrets_to_json(p.secrets), -1);
    auto blob = protector_.protect(secrets);
    if (!blob) {
        return std::unexpected("protect secrets failed");
    }
    if (auto r = write_file_atomic(secure_dir() / (p.id + ".bin"), *blob); !r) {
        return r;
    }
    return write_file_atomic(profiles_dir() / (p.id + ".json"), json::serialize(profile_to_json(p)) + "\n");
}

std::expected<void, std::string> ProfileStore::remove(std::string_view id) {
    if (!is_profile_id(id)) {
        return std::unexpected("bad profile id");
    }
    std::error_code ec;
    const std::string name(id);
    std::filesystem::remove(profiles_dir() / (name + ".json"), ec);
    if (ec) {
        return std::unexpected("remove profile: " + ec.message());
    }
    std::filesystem::remove(secure_dir() / (name + ".bin"), ec);
    return {};
}

std::expected<Profile, std::string> ProfileStore::clone(const Profile& src,
                                                        const std::vector<std::string>& taken) {
    Profile copy = src;
    copy.id = new_profile_id();
    copy.name = unique_name(src.name, taken);
    if (auto r = save(copy); !r) {
        return std::unexpected(r.error());
    }
    return copy;
}

} // namespace ft
