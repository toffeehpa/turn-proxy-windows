#include "win/paths.h"

#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>

#include "win/handle.h"

namespace ft::win {

namespace {

std::filesystem::path known_folder(const KNOWNFOLDERID& id) {
    PWSTR raw = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &raw);
    CoTaskPtr<wchar_t> owned(raw);
    if (SUCCEEDED(hr) && owned) {
        return owned.get();
    }
    return std::filesystem::temp_directory_path();
}

} // namespace

std::filesystem::path app_data_dir() {
    return known_folder(FOLDERID_LocalAppData) / L"FreeTurn";
}

std::filesystem::path program_data_dir() {
    return known_folder(FOLDERID_ProgramData) / L"FreeTurn";
}

std::filesystem::path exe_dir() {
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0) {
            return {};
        }
        if (n < buf.size()) {
            buf.resize(n);
            break;
        }
        buf.resize(buf.size() * 2);
    }
    return std::filesystem::path(buf).parent_path();
}

} // namespace ft::win
