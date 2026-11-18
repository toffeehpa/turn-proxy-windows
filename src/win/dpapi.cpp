#include "win/dpapi.h"

#include <windows.h>
#include <wincrypt.h>

#include "win/handle.h"

namespace ft::win {

namespace {

// Энтропия отделяет наши blob-ы от чужих. От программ того же пользователя это не защита:
// строка открыта, граница безопасности - учётная запись Windows.
constexpr char kEntropy[] = "FreeTurn.secrets.v1";

DATA_BLOB entropy_blob() {
    DATA_BLOB b{};
    b.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(kEntropy));
    b.cbData = sizeof(kEntropy) - 1;
    return b;
}

std::optional<std::string> take(DATA_BLOB& out) {
    LocalPtr<BYTE> owned(out.pbData);
    std::string result(reinterpret_cast<const char*>(owned.get()), out.cbData);
    SecureZeroMemory(owned.get(), out.cbData);
    return result;
}

} // namespace

std::optional<std::string> DpapiProtector::protect(std::string_view plain) {
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()));
    in.cbData = static_cast<DWORD>(plain.size());
    DATA_BLOB ent = entropy_blob();
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"FreeTurn", &ent, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return std::nullopt;
    }
    return take(out);
}

std::optional<std::string> DpapiProtector::unprotect(std::string_view blob) {
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(blob.data()));
    in.cbData = static_cast<DWORD>(blob.size());
    DATA_BLOB ent = entropy_blob();
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, &ent, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return std::nullopt;
    }
    return take(out);
}

} // namespace ft::win
