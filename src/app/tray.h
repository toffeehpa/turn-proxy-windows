#pragma once

#include <windows.h>
#include <shellapi.h>

#include <string>

namespace ft::ui {

// Иконка в области уведомлений. Проводник может перезапуститься - тогда add() заново.
class TrayIcon {
public:
    bool add(HWND hwnd, UINT callback_msg, HICON icon, const std::wstring& tip);
    void update(HICON icon, const std::wstring& tip);
    void balloon(const std::wstring& title, const std::wstring& text, bool error = false);
    void remove();

private:
    NOTIFYICONDATAW base() const;

    HWND hwnd_ = nullptr;
    bool added_ = false;
};

// Меню у курсора: без SetForegroundWindow оно не закрывается по клику мимо.
void show_tray_menu(HWND hwnd, HMENU menu, POINT pt);

} // namespace ft::ui
