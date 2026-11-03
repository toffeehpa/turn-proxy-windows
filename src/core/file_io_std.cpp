#include "core/file_io.h"

#include <fstream>
#include <system_error>

namespace ft {

std::expected<void, std::string> write_file_atomic(const std::filesystem::path& file, std::string_view data) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    auto tmp = file;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!out.flush()) {
            return std::unexpected("write " + tmp.filename().string());
        }
    }
    std::filesystem::rename(tmp, file, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return std::unexpected("rename " + file.filename().string());
    }
    return {};
}

} // namespace ft
