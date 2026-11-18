#pragma once

namespace ft::win {

inline constexpr const wchar_t* kServiceName = L"FreeTurnService";
inline constexpr const wchar_t* kServicePipe = L"\\\\.\\pipe\\FreeTurnService";
inline constexpr const wchar_t* kLocalSystemSid = L"S-1-5-18";
// SYSTEM и администраторы - полный доступ, интерактивные пользователи - только чтение и запись сообщений.
inline constexpr const wchar_t* kServicePipeSddl = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;IU)";

} // namespace ft::win
