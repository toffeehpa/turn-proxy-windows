#include "core/file_io.h"

#include <windows.h>

#include "win/handle.h"
#include "win/wstr.h"

namespace ft {

std::expected<void, std::string> write_file_atomic(const std::filesystem::path& file, std::string_view data) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    auto tmp = file;
    tmp += L".tmp";
    win::UniqueHandle h = win::make_handle(
        CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!h) {
        return std::unexpected("create " + tmp.filename().string() + ": " +
                               win::last_error_text(GetLastError()));
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(h.get(), data.data(), static_cast<DWORD>(data.size()), &written, nullptr) &&
                    written == data.size() && FlushFileBuffers(h.get());
    const DWORD err = GetLastError();
    h.reset();
    if (!ok) {
        DeleteFileW(tmp.c_str());
        return std::unexpected("write " + tmp.filename().string() + ": " + win::last_error_text(err));
    }
    // Сброс на диск до замены: после сбоя питания остаётся старый файл или новый целиком.
    if (!MoveFileExW(tmp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD move_err = GetLastError();
        DeleteFileW(tmp.c_str());
        return std::unexpected("replace " + file.filename().string() + ": " + win::last_error_text(move_err));
    }
    return {};
}

} // namespace ft
