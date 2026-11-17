#pragma once

#include <windows.h>

#include <filesystem>
#include <string>

#include "core/connection_manager.h"
#include "core/profile.h"
#include "core/settings.h"

namespace ft::ui {

struct ProfileDialogOptions {
    bool import_mode = false;
    bool allow_sfu = false;
    // Ссылка из командной строки или второго экземпляра: подставляется, но не сохраняется без OK.
    std::string initial_link;
    // Цвета в списке звонков, если профиль сейчас подключён с роллингом.
    std::vector<CallStatus> calls;
};

bool profile_dialog(HINSTANCE inst, HWND owner, Profile& p, const ProfileDialogOptions& opts);
bool settings_dialog(HINSTANCE inst, HWND owner, Settings& s, const std::filesystem::path& data_dir,
                     const std::filesystem::path& log_file);

int message_box(HWND owner, const std::wstring& text, UINT flags);

} // namespace ft::ui
