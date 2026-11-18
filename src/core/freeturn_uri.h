#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "core/profile.h"

namespace ft {

inline constexpr std::string_view kUriScheme = "freeturn://";
inline constexpr int kUriVersion = 1;
// Ссылка влезает в буфер обмена и командную строку, больше - точно мусор.
inline constexpr std::size_t kMaxUriLength = 16 * 1024;

enum class UriError {
    BadScheme,
    Empty,
    TooLong,
    BadBase64,
    BadJson,
    UnsupportedVersion,
    MissingProvider,
    MissingPeer
};

std::string_view to_string(UriError e);

// Разбор docs/uri.md ядра. Имя профиля берётся из поля name, ссылка на звонок - из vk.
std::expected<Profile, UriError> parse_freeturn_uri(std::string_view uri);

// Обратно в ссылку: только поля из uri.md, дефолты опускаются.
std::string build_freeturn_uri(const Profile& p);

} // namespace ft
