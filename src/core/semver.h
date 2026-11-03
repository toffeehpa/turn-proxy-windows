#pragma once

#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace ft {

// SemVer 2.0 без build-метаданных: 0.1.0-beta.1 < 0.1.0-beta.2 < 0.1.0.
struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string pre;

    std::strong_ordering operator<=>(const Version& other) const;
    bool operator==(const Version& other) const = default;
    std::string str() const;
};

// Тег релиза: допускает префикс v, как vX.Y.Z у release-please.
std::optional<Version> parse_version(std::string_view text);

} // namespace ft
