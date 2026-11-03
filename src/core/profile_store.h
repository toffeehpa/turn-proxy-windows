#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/file_io.h"
#include "core/profile.h"

namespace ft {

// Шифрование секретов. На Windows - DPAPI (CurrentUser), в тестах - подделка.
class ISecretProtector {
public:
    virtual ~ISecretProtector() = default;
    virtual std::optional<std::string> protect(std::string_view plain) = 0;
    virtual std::optional<std::string> unprotect(std::string_view blob) = 0;
};

struct StoreLoadResult {
    std::vector<Profile> profiles;
    // Имена файлов, которые не удалось прочитать: показать пользователю, не падать.
    std::vector<std::string> broken;
};

// Профили лежат по файлу на штуку: profiles\<id>.json и secure\<id>.bin.
// Обнаружение - перечисление каталога, как конфиги в OpenVPN GUI, без реестра.
class ProfileStore {
public:
    ProfileStore(std::filesystem::path root, ISecretProtector& protector);

    StoreLoadResult load_all();
    std::expected<void, std::string> save(Profile& p);
    std::expected<void, std::string> remove(std::string_view id);

    std::expected<Profile, std::string> clone(const Profile& src,
                                              const std::vector<std::string>& taken_names);

    std::filesystem::path profiles_dir() const { return root_ / "profiles"; }
    std::filesystem::path secure_dir() const { return root_ / "secure"; }

private:
    std::filesystem::path root_;
    ISecretProtector& protector_;
};

} // namespace ft
