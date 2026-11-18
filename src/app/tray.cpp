#include "app/tray.h"

namespace ft::ui {

namespace {

constexpr UINT kIconId = 1;

void copy_text(wchar_t* dst, std::size_t cap, const std::wstring& src) {
    const std::size_t n = src.size() < cap - 1 ? src.size() : cap - 1;
    src.copy(dst, n);
    dst[n] = L'\0';
}

} // namespace

NOTIFYICONDATAW TrayIcon::base() const {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = kIconId;
    return nid;
}

bool TrayIcon::add(HWND hwnd, UINT callback_msg, HICON icon, const std::wstring& tip) {
    hwnd_ = hwnd;
    NOTIFYICONDATAW nid = base();
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = callback_msg;
    nid.hIcon = icon;
    copy_text(nid.szTip, std::size(nid.szTip), tip);
    added_ = Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
    if (added_) {
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid);
    }
    return added_;
}

void TrayIcon::update(HICON icon, const std::wstring& tip) {
    if (!added_) {
        return;
    }
    NOTIFYICONDATAW nid = base();
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.hIcon = icon;
    copy_text(nid.szTip, std::size(nid.szTip), tip);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayIcon::balloon(const std::wstring& title, const std::wstring& text, bool error) {
    if (!added_) {
        return;
    }
    NOTIFYICONDATAW nid = base();
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = (error ? NIIF_ERROR : NIIF_INFO) | NIIF_RESPECT_QUIET_TIME;
    copy_text(nid.szInfoTitle, std::size(nid.szInfoTitle), title);
    copy_text(nid.szInfo, std::size(nid.szInfo), text);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayIcon::remove() {
    if (!added_) {
        return;
    }
    NOTIFYICONDATAW nid = base();
    Shell_NotifyIconW(NIM_DELETE, &nid);
    added_ = false;
}

void show_tray_menu(HWND hwnd, HMENU menu, POINT pt) {
    SetForegroundWindow(hwnd);
    const UINT align = GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0 ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
    TrackPopupMenuEx(menu, align | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
}

} // namespace ft::ui
