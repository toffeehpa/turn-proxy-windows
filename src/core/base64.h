#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ft::base64 {

// RawURLEncoding как в Go: алфавит -_ и без '='.
std::string encode_url(std::string_view data);

// Принимает и url-, и std-алфавит, с padding и без: ссылки копируют откуда попало.
std::optional<std::string> decode(std::string_view text);

} // namespace ft::base64
