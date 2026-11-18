#include "core/base64.h"

#include <array>
#include <cstdint>

namespace ft::base64 {

namespace {

constexpr std::string_view kUrlAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

constexpr std::array<std::int8_t, 256> make_table() {
    std::array<std::int8_t, 256> t{};
    for (auto& v : t) {
        v = -1;
    }
    for (std::size_t i = 0; i < kUrlAlphabet.size(); ++i) {
        t[static_cast<unsigned char>(kUrlAlphabet[i])] = static_cast<std::int8_t>(i);
    }
    t[static_cast<unsigned char>('+')] = 62;
    t[static_cast<unsigned char>('/')] = 63;
    return t;
}

constexpr auto kTable = make_table();

} // namespace

std::string encode_url(std::string_view data) {
    std::string out;
    out.reserve((data.size() * 4 + 2) / 3);
    std::size_t i = 0;
    for (; i + 3 <= data.size(); i += 3) {
        const std::uint32_t n = (static_cast<std::uint32_t>(static_cast<unsigned char>(data[i])) << 16) |
                                (static_cast<std::uint32_t>(static_cast<unsigned char>(data[i + 1])) << 8) |
                                static_cast<std::uint32_t>(static_cast<unsigned char>(data[i + 2]));
        out.push_back(kUrlAlphabet[(n >> 18) & 63]);
        out.push_back(kUrlAlphabet[(n >> 12) & 63]);
        out.push_back(kUrlAlphabet[(n >> 6) & 63]);
        out.push_back(kUrlAlphabet[n & 63]);
    }
    const std::size_t rest = data.size() - i;
    if (rest == 1) {
        const std::uint32_t n = static_cast<std::uint32_t>(static_cast<unsigned char>(data[i])) << 16;
        out.push_back(kUrlAlphabet[(n >> 18) & 63]);
        out.push_back(kUrlAlphabet[(n >> 12) & 63]);
    } else if (rest == 2) {
        const std::uint32_t n = (static_cast<std::uint32_t>(static_cast<unsigned char>(data[i])) << 16) |
                                (static_cast<std::uint32_t>(static_cast<unsigned char>(data[i + 1])) << 8);
        out.push_back(kUrlAlphabet[(n >> 18) & 63]);
        out.push_back(kUrlAlphabet[(n >> 12) & 63]);
        out.push_back(kUrlAlphabet[(n >> 6) & 63]);
    }
    return out;
}

std::optional<std::string> decode(std::string_view text) {
    while (!text.empty() && text.back() == '=') {
        text.remove_suffix(1);
    }
    if (text.size() % 4 == 1) {
        return std::nullopt;
    }
    std::string out;
    out.reserve(text.size() * 3 / 4);
    std::uint32_t acc = 0;
    int bits = 0;
    for (const char c : text) {
        const std::int8_t v = kTable[static_cast<unsigned char>(c)];
        if (v < 0) {
            return std::nullopt;
        }
        acc = (acc << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((acc >> bits) & 0xFF));
        }
    }
    return out;
}

} // namespace ft::base64
