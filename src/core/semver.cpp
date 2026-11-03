#include "core/semver.h"

#include <charconv>
#include <format>
#include <vector>

namespace ft {

namespace {

bool parse_num(std::string_view s, int& out) {
    if (s.empty() || (s.size() > 1 && s.front() == '0') || s.size() > 9) {
        return false;
    }
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc() && ptr == s.data() + s.size();
}

bool is_numeric(std::string_view s) {
    return !s.empty() && s.find_first_not_of("0123456789") == std::string_view::npos;
}

std::vector<std::string_view> split_dots(std::string_view s) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    for (;;) {
        const auto dot = s.find('.', start);
        out.push_back(s.substr(start, dot - start));
        if (dot == std::string_view::npos) {
            return out;
        }
        start = dot + 1;
    }
}

std::strong_ordering compare_pre(std::string_view a, std::string_view b) {
    // Без пре-релиза версия старше любой своей беты.
    if (a.empty() || b.empty()) {
        return a.empty() <=> b.empty();
    }
    const auto pa = split_dots(a);
    const auto pb = split_dots(b);
    for (std::size_t i = 0; i < pa.size() && i < pb.size(); ++i) {
        const bool na = is_numeric(pa[i]);
        const bool nb = is_numeric(pb[i]);
        if (na && nb) {
            if (pa[i].size() != pb[i].size()) {
                return pa[i].size() <=> pb[i].size();
            }
            if (const auto c = pa[i].compare(pb[i]); c != 0) {
                return c <=> 0;
            }
        } else if (na != nb) {
            return na ? std::strong_ordering::less : std::strong_ordering::greater;
        } else if (const auto c = pa[i].compare(pb[i]); c != 0) {
            return c <=> 0;
        }
    }
    return pa.size() <=> pb.size();
}

} // namespace

std::strong_ordering Version::operator<=>(const Version& o) const {
    if (const auto c = major <=> o.major; c != 0) {
        return c;
    }
    if (const auto c = minor <=> o.minor; c != 0) {
        return c;
    }
    if (const auto c = patch <=> o.patch; c != 0) {
        return c;
    }
    return compare_pre(pre, o.pre);
}

std::string Version::str() const {
    return pre.empty() ? std::format("{}.{}.{}", major, minor, patch)
                       : std::format("{}.{}.{}-{}", major, minor, patch, pre);
}

std::optional<Version> parse_version(std::string_view text) {
    if (text.starts_with('v') || text.starts_with('V')) {
        text.remove_prefix(1);
    }
    if (const auto plus = text.find('+'); plus != std::string_view::npos) {
        text = text.substr(0, plus);
    }
    Version v;
    const auto dash = text.find('-');
    if (dash != std::string_view::npos) {
        v.pre = std::string(text.substr(dash + 1));
        text = text.substr(0, dash);
        for (const auto part : split_dots(v.pre)) {
            if (part.empty() ||
                part.find_first_not_of("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-") !=
                    std::string_view::npos) {
                return std::nullopt;
            }
        }
    }
    const auto parts = split_dots(text);
    if (parts.size() != 3 || !parse_num(parts[0], v.major) || !parse_num(parts[1], v.minor) ||
        !parse_num(parts[2], v.patch)) {
        return std::nullopt;
    }
    return v;
}

} // namespace ft
