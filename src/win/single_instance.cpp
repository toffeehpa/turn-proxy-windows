#include "win/single_instance.h"

#include <sddl.h>

#include <vector>

namespace ft::win {

std::wstring current_user_sid() {
    HANDLE raw_token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw_token)) {
        return {};
    }
    UniqueHandle token = make_handle(raw_token);
    DWORD size = 0;
    GetTokenInformation(token.get(), TokenUser, nullptr, 0, &size);
    std::vector<BYTE> buf(size);
    LPWSTR raw_str = nullptr;
    if (size == 0 || !GetTokenInformation(token.get(), TokenUser, buf.data(), size, &size) ||
        !ConvertSidToStringSidW(reinterpret_cast<const TOKEN_USER*>(buf.data())->User.Sid, &raw_str)) {
        return {};
    }
    LocalPtr<wchar_t> str(raw_str);
    return str.get();
}

SingleInstance::SingleInstance() {
    const std::wstring name = L"Local\\FreeTurn.SingleInstance." + current_user_sid();
    mutex_ = make_handle(CreateMutexW(nullptr, TRUE, name.c_str()));
    first_ = mutex_ && GetLastError() != ERROR_ALREADY_EXISTS;
}

SingleInstance::~SingleInstance() {
    if (first_) {
        ReleaseMutex(mutex_.get());
    }
}

} // namespace ft::win
