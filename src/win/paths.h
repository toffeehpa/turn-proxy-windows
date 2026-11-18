#pragma once

#include <filesystem>

namespace ft::win {

inline constexpr const wchar_t* kCoreExeName = L"freeturn-core.exe";

// %LOCALAPPDATA%\FreeTurn: profiles, secure, settings.json, logs, updates.
std::filesystem::path app_data_dir();
// %ProgramData%\FreeTurn: рабочая папка и лог службы.
std::filesystem::path program_data_dir();
std::filesystem::path exe_dir();

} // namespace ft::win
