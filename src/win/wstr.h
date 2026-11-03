#pragma once

#include <string>
#include <string_view>

namespace ft::win {

std::wstring widen(std::string_view utf8);
std::string narrow(std::wstring_view utf16);

// Текст ошибки Win32 для лога, по-английски: системный язык в логе мешает искать.
std::string last_error_text(unsigned long code);

} // namespace ft::win
