#include "core/file_io.h"

#include <fstream>
#include <iterator>
#include <system_error>

namespace ft {

std::optional<std::string> read_file(const std::filesystem::path& file, std::uintmax_t max_size) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(file, ec);
    if (ec || size > max_size) {
        return std::nullopt;
    }
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace ft
