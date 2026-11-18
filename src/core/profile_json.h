#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "core/json.h"
#include "core/profile.h"

namespace ft {

inline constexpr int kProfileFormatVersion = 1;

// Открытая часть профиля, без секретов: profiles\<id>.json.
json::Value profile_to_json(const Profile& p);
// Секреты - отдельным blob-ом, который на Windows шифрует DPAPI.
json::Value secrets_to_json(const ProfileSecrets& s);

// Незнакомые ключи игнорируем: файл мог записать более новый FreeTurn.
std::expected<Profile, std::string> profile_from_json(const json::Value& v);
std::expected<ProfileSecrets, std::string> secrets_from_json(const json::Value& v);

// Импорт JSON-файла: схема ядра (ClientJSON из internal/config/json.go) или свой формат.
std::expected<Profile, std::string> import_profile_json(std::string_view text);

} // namespace ft
