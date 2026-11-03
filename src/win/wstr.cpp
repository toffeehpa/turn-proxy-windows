#include "win/wstr.h"

#include <windows.h>

#include <format>

#include "win/handle.h"

namespace ft::win {

std::wstring widen(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (n <= 0) {
        return {};
    }
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
    return out;
}

std::string narrow(std::wstring_view utf16) {
    if (utf16.empty()) {
        return {};
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), nullptr, 0,
                                      nullptr, nullptr);
    if (n <= 0) {
        return {};
    }
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), out.data(), n, nullptr,
                        nullptr);
    return out;
}

std::string last_error_text(unsigned long code) {
    wchar_t* buf = nullptr;
    const DWORD len = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
        code, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), reinterpret_cast<wchar_t*>(&buf), 0, nullptr);
    LocalPtr<wchar_t> owned(buf);
    std::string text;
    if (len > 0 && owned) {
        text = narrow(std::wstring_view(owned.get(), len));
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
            text.pop_back();
        }
    }
    return std::format("{} (0x{:08X})", text.empty() ? "error" : text, code);
}

} // namespace ft::win
