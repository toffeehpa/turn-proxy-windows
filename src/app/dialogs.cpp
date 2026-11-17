#include "app/dialogs.h"

#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>

#include <array>
#include <cwchar>
#include <optional>
#include <string_view>

#include "app/strings.h"
#include "core/freeturn_uri.h"
#include "core/profile_json.h"
#include "core/profile_store.h"
#include "core/version.h"
#include "resource.h"
#include "win/handle.h"
#include "win/wstr.h"

namespace ft::ui {

namespace {

// Окна следуют теме приложений Windows; своей палитры нет - только системные нейтральные цвета.
struct Theme {
    bool dark = false;
    win::UniqueBrush bg;
    win::UniqueBrush ctl;

    static constexpr COLORREF kBg = RGB(32, 32, 32);
    static constexpr COLORREF kCtl = RGB(45, 45, 45);
    static constexpr COLORREF kText = RGB(242, 242, 242);
};

bool apps_use_dark_theme() {
    HIGHCONTRASTW hc{};
    hc.cbSize = sizeof(hc);
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0) &&
        (hc.dwFlags & HCF_HIGHCONTRASTON) != 0) {
        return false;
    }
    DWORD value = 1;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) {
        return false;
    }
    return value == 0;
}

BOOL CALLBACK theme_child(HWND child, LPARAM) {
    wchar_t cls[32] = {};
    GetClassNameW(child, cls, static_cast<int>(std::size(cls)));
    const std::wstring_view name = cls;
    if (name == L"Button") {
        const auto type = GetWindowLongPtrW(child, GWL_STYLE) & BS_TYPEMASK;
        // Темизированные чекбокс и рамка группы рисуют подпись чёрным и игнорируют WM_CTLCOLOR - берём
        // классические.
        if (type == BS_AUTOCHECKBOX || type == BS_CHECKBOX || type == BS_GROUPBOX) {
            SetWindowTheme(child, L"", L"");
        } else {
            SetWindowTheme(child, L"DarkMode_Explorer", nullptr);
        }
    } else if (name == L"Edit") {
        SetWindowTheme(child, L"DarkMode_Explorer", nullptr);
    } else if (name == L"ComboBox") {
        SetWindowTheme(child, L"DarkMode_CFD", nullptr);
    }
    return TRUE;
}

void apply_theme(HWND dlg, Theme& t) {
    t.dark = apps_use_dark_theme();
    if (!t.dark) {
        return;
    }
    t.bg.reset(CreateSolidBrush(Theme::kBg));
    t.ctl.reset(CreateSolidBrush(Theme::kCtl));
    const BOOL on = TRUE;
    constexpr DWORD kUseImmersiveDarkMode = 20;
    DwmSetWindowAttribute(dlg, kUseImmersiveDarkMode, &on, sizeof(on));
    EnumChildWindows(dlg, theme_child, 0);
}

INT_PTR theme_color(const Theme& t, UINT msg, WPARAM wp) {
    if (!t.dark) {
        return 0;
    }
    auto* dc = reinterpret_cast<HDC>(wp);
    switch (msg) {
    case WM_CTLCOLORDLG:
        return reinterpret_cast<INT_PTR>(t.bg.get());
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetTextColor(dc, Theme::kText);
        SetBkColor(dc, Theme::kBg);
        return reinterpret_cast<INT_PTR>(t.bg.get());
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        SetTextColor(dc, Theme::kText);
        SetBkColor(dc, Theme::kCtl);
        return reinterpret_cast<INT_PTR>(t.ctl.get());
    default:
        return 0;
    }
}

std::wstring get_text(HWND dlg, int id) {
    HWND h = GetDlgItem(dlg, id);
    const int len = GetWindowTextLengthW(h);
    std::wstring s(static_cast<std::size_t>(len) + 1, L'\0');
    GetWindowTextW(h, s.data(), len + 1);
    s.resize(static_cast<std::size_t>(len));
    return s;
}

std::string get_utf8(HWND dlg, int id) {
    std::string s = win::narrow(get_text(dlg, id));
    while (!s.empty() && static_cast<unsigned char>(s.back()) <= 0x20) {
        s.pop_back();
    }
    std::size_t start = 0;
    while (start < s.size() && static_cast<unsigned char>(s[start]) <= 0x20) {
        ++start;
    }
    return s.substr(start);
}

void set_utf8(HWND dlg, int id, std::string_view text) {
    SetDlgItemTextW(dlg, id, win::widen(text).c_str());
}

void set_label(HWND dlg, int id, S s) {
    SetDlgItemTextW(dlg, id, tr(s));
}

void check(HWND dlg, int id, bool on) {
    CheckDlgButton(dlg, id, on ? BST_CHECKED : BST_UNCHECKED);
}

bool checked(HWND dlg, int id) {
    return IsDlgButtonChecked(dlg, id) == BST_CHECKED;
}

struct Choice {
    std::string_view value;
    S label = S::Count;
};

constexpr std::array kProviders = {Choice{"vk", S::ProviderVk}, Choice{"direct", S::ProviderDirect},
                                   Choice{"sfu", S::ProviderSfu}};
constexpr std::array kModes = {Choice{"udp", S::ModeUdp}, Choice{"tcp", S::ModeTcp}};
constexpr std::array kTransports = {Choice{"tcp"}, Choice{"udp"}};
constexpr std::array kObf = {Choice{"none", S::ObfNone}, Choice{"rtpopus"}, Choice{"rtpopus2"},
                             Choice{"rtpopus3"}};

template <std::size_t N>
void fill_combo(HWND dlg, int id, const std::array<Choice, N>& items, std::string_view current,
                std::size_t limit = N) {
    HWND h = GetDlgItem(dlg, id);
    SendMessageW(h, CB_RESETCONTENT, 0, 0);
    int selected = 0;
    for (std::size_t i = 0; i < limit && i < N; ++i) {
        const std::wstring label =
            items[i].label == S::Count ? win::widen(items[i].value) : std::wstring(tr(items[i].label));
        const auto idx = SendMessageW(h, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        SendMessageW(h, CB_SETITEMDATA, static_cast<WPARAM>(idx), static_cast<LPARAM>(i));
        if (items[i].value == current) {
            selected = static_cast<int>(idx);
        }
    }
    SendMessageW(h, CB_SETCURSEL, static_cast<WPARAM>(selected), 0);
}

template <std::size_t N>
std::string combo_value(HWND dlg, int id, const std::array<Choice, N>& items) {
    HWND h = GetDlgItem(dlg, id);
    const auto sel = SendMessageW(h, CB_GETCURSEL, 0, 0);
    if (sel < 0) {
        return std::string(items[0].value);
    }
    const auto i = static_cast<std::size_t>(SendMessageW(h, CB_GETITEMDATA, static_cast<WPARAM>(sel), 0));
    return std::string(i < N ? items[i].value : items[0].value);
}

std::optional<std::string> clipboard_text(HWND owner) {
    win::ClipboardLock clipboard(owner);
    if (!clipboard.ok()) {
        return std::nullopt;
    }
    std::optional<std::string> out;
    if (HANDLE data = GetClipboardData(CF_UNICODETEXT); data != nullptr) {
        if (const auto* text = static_cast<const wchar_t*>(GlobalLock(data)); text != nullptr) {
            out = win::narrow(std::wstring_view(text, wcsnlen(text, kMaxUriLength + 16)));
            GlobalUnlock(data);
        }
    }
    return out;
}

std::optional<std::filesystem::path> open_file(HWND owner, const wchar_t* filter,
                                               const std::wstring& initial) {
    std::wstring buf(MAX_PATH * 4, L'\0');
    if (!initial.empty() && initial.size() < buf.size()) {
        initial.copy(buf.data(), initial.size());
    }
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf.data();
    ofn.nMaxFile = static_cast<DWORD>(buf.size());
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT;
    if (!GetOpenFileNameW(&ofn)) {
        return std::nullopt;
    }
    return std::filesystem::path(buf.c_str());
}

std::wstring make_filter(S label, const wchar_t* pattern) {
    std::wstring f = tr(label);
    f.push_back(L'\0');
    f += pattern;
    f.push_back(L'\0');
    f.push_back(L'\0');
    return f;
}

struct ProfileCtx {
    Profile p;
    ProfileDialogOptions opts;
    Theme theme;
    bool loading = false;
    bool obf = false;
};

void load_fields(HWND dlg, ProfileCtx& ctx) {
    ctx.loading = true;
    const Profile& p = ctx.p;
    set_utf8(dlg, IDC_NAME, p.name);
    const bool sfu_visible = ctx.opts.allow_sfu || p.provider == kProviderSfu;
    fill_combo(dlg, IDC_PROVIDER, kProviders, p.provider, sfu_visible ? kProviders.size() : 2);
    set_utf8(dlg, IDC_PEER, p.peer);
    std::string links;
    for (const auto& l : p.secrets.links) {
        if (!links.empty()) {
            links += "\r\n";
        }
        links += l;
    }
    set_utf8(dlg, IDC_LINKS, links);
    set_utf8(dlg, IDC_LISTEN, p.listen);
    fill_combo(dlg, IDC_MODE, kModes, p.mode);
    fill_combo(dlg, IDC_TRANSPORT, kTransports, p.transport);
    SetDlgItemInt(dlg, IDC_STREAMS, static_cast<UINT>(p.streams), FALSE);
    fill_combo(dlg, IDC_OBF, kObf, p.obf_profile);
    set_utf8(dlg, IDC_OBF_KEY, p.secrets.obf_key);
    SetDlgItemInt(dlg, IDC_OBF_TIMING, static_cast<UINT>(p.obf_timing_ms), FALSE);
    ctx.obf = p.obf_profile != kObfNone;
    set_utf8(dlg, IDC_CLIENT_ID, p.secrets.client_id);
    check(dlg, IDC_MANUAL_CAPTCHA, p.manual_captcha);
    check(dlg, IDC_BOND, p.bond);
    ctx.loading = false;
}

void read_fields(HWND dlg, Profile& p) {
    p.name = sanitize_name(get_utf8(dlg, IDC_NAME));
    p.provider = combo_value(dlg, IDC_PROVIDER, kProviders);
    p.peer = get_utf8(dlg, IDC_PEER);
    p.secrets.links.clear();
    const std::string links = get_utf8(dlg, IDC_LINKS);
    std::size_t start = 0;
    while (start <= links.size()) {
        auto end = links.find_first_of("\r\n", start);
        if (end == std::string::npos) {
            end = links.size();
        }
        std::string line = links.substr(start, end - start);
        while (!line.empty() && line.back() == ' ') {
            line.pop_back();
        }
        if (const auto first = line.find_first_not_of(' '); first != std::string::npos) {
            p.secrets.links.push_back(line.substr(first));
        }
        start = end + 1;
    }
    p.listen = get_utf8(dlg, IDC_LISTEN);
    p.mode = combo_value(dlg, IDC_MODE, kModes);
    p.transport = combo_value(dlg, IDC_TRANSPORT, kTransports);
    BOOL ok = FALSE;
    const UINT streams = GetDlgItemInt(dlg, IDC_STREAMS, &ok, FALSE);
    p.streams = ok ? static_cast<int>(streams) : 0;
    p.obf_profile = combo_value(dlg, IDC_OBF, kObf);
    p.secrets.obf_key = get_utf8(dlg, IDC_OBF_KEY);
    const UINT timing = GetDlgItemInt(dlg, IDC_OBF_TIMING, &ok, FALSE);
    p.obf_timing_ms = ok && p.obf_profile != kObfNone ? static_cast<int>(timing) : 0;
    p.secrets.client_id = get_utf8(dlg, IDC_CLIENT_ID);
    p.manual_captcha = checked(dlg, IDC_MANUAL_CAPTCHA);
    p.bond = checked(dlg, IDC_BOND);
}

void merge_imported(HWND dlg, ProfileCtx& ctx, Profile imported) {
    Profile current = ctx.p;
    read_fields(dlg, current);
    imported.id = current.id;
    if (imported.name.empty()) {
        imported.name = current.name;
    }
    if (imported.secrets.links.empty()) {
        imported.secrets.links = current.secrets.links;
    }
    ctx.p = std::move(imported);
    load_fields(dlg, ctx);
}

// DS_CENTER ставит окно в центр экрана, а «О программе» открывается поверх настроек.
void center_on_owner(HWND dlg) {
    RECT owner{};
    RECT self{};
    GetWindowRect(GetWindow(dlg, GW_OWNER), &owner);
    GetWindowRect(dlg, &self);
    const int w = self.right - self.left;
    const int h = self.bottom - self.top;
    SetWindowPos(dlg, nullptr, owner.left + ((owner.right - owner.left) - w) / 2,
                 owner.top + ((owner.bottom - owner.top) - h) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void add_tooltip(HWND dlg, int id, const wchar_t* text) {
    HWND tip = CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, dlg, nullptr, GetModuleHandleW(nullptr), nullptr);
    TOOLINFOW ti{};
    ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd = dlg;
    ti.uId = reinterpret_cast<UINT_PTR>(GetDlgItem(dlg, id));
    ti.lpszText = const_cast<wchar_t*>(text);
    SendMessageW(tip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
    SendMessageW(tip, TTM_SETMAXTIPWIDTH, 0, 320);
    SendMessageW(tip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 30'000);
}

bool try_link(HWND dlg, ProfileCtx& ctx, bool report) {
    const std::string link = get_utf8(dlg, IDC_LINK);
    auto parsed = parse_freeturn_uri(link);
    if (!parsed) {
        if (report) {
            message_box(dlg, trf(S::ImportFailed, {uri_error_text(parsed.error())}), MB_ICONWARNING);
        }
        return false;
    }
    merge_imported(dlg, ctx, std::move(*parsed));
    return true;
}

void import_file(HWND dlg, ProfileCtx& ctx) {
    const auto filter = make_filter(S::FilterJson, L"*.json");
    auto path = open_file(dlg, filter.c_str(), {});
    if (!path) {
        return;
    }
    auto text = read_file(*path, 1024 * 1024);
    auto parsed = text ? import_profile_json(*text) : std::unexpected(std::string("read"));
    if (!parsed) {
        message_box(dlg, trf(S::ImportFileFailed, {win::widen(parsed.error())}), MB_ICONWARNING);
        return;
    }
    if (parsed->name.empty()) {
        parsed->name = win::narrow(path->stem().wstring());
    }
    merge_imported(dlg, ctx, std::move(*parsed));
}

bool accept_profile(HWND dlg, ProfileCtx& ctx) {
    Profile p = ctx.p;
    read_fields(dlg, p);
    if (p.provider == kProviderSfu && !ctx.opts.allow_sfu) {
        message_box(dlg, tr(S::ExperimentalDisabled), MB_ICONINFORMATION);
        return false;
    }
    if (const auto issues = validate(p); !issues.empty()) {
        std::wstring list;
        for (const auto i : issues) {
            if (!list.empty()) {
                list += L"; ";
            }
            list += issue_text(i);
        }
        message_box(dlg, trf(S::InvalidProfile, {list}), MB_ICONWARNING);
        return false;
    }
    ctx.p = std::move(p);
    return true;
}

INT_PTR CALLBACK profile_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    auto* ctx = reinterpret_cast<ProfileCtx*>(GetWindowLongPtrW(dlg, DWLP_USER));
    switch (msg) {
    case WM_INITDIALOG: {
        ctx = reinterpret_cast<ProfileCtx*>(lp);
        SetWindowLongPtrW(dlg, DWLP_USER, lp);
        SetWindowTextW(dlg, tr(ctx->opts.import_mode ? S::DlgImportTitle : S::DlgProfileTitle));
        set_label(dlg, IDC_LBL_LINK, S::DlgLink);
        set_label(dlg, IDC_PASTE, S::DlgPaste);
        set_label(dlg, IDC_FROM_FILE, S::DlgFromFile);
        set_label(dlg, IDC_LBL_NAME, S::DlgName);
        set_label(dlg, IDC_LBL_PROVIDER, S::DlgProvider);
        set_label(dlg, IDC_LBL_PEER, S::DlgPeer);
        set_label(dlg, IDC_LBL_LINKS, S::DlgLinks);
        set_label(dlg, IDC_LBL_LISTEN, S::DlgListen);
        set_label(dlg, IDC_LBL_MODE, S::DlgMode);
        set_label(dlg, IDC_LBL_TRANSPORT, S::DlgTransport);
        set_label(dlg, IDC_LBL_STREAMS, S::DlgStreams);
        set_label(dlg, IDC_LBL_OBF, S::DlgObf);
        set_label(dlg, IDC_LBL_OBF_KEY, S::DlgObfKey);
        set_label(dlg, IDC_LBL_OBF_TIMING, S::DlgObfTiming);
        set_label(dlg, IDC_OBF_UNIT, S::DlgObfTimingUnit);
        add_tooltip(dlg, IDC_OBF_TIMING, tr(S::DlgObfTimingHint));
        SendDlgItemMessageW(dlg, IDC_LINK, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"freeturn://"));
        SendDlgItemMessageW(dlg, IDC_PEER, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"host:port"));
        set_label(dlg, IDC_LBL_CLIENT_ID, S::DlgClientId);
        set_label(dlg, IDC_MANUAL_CAPTCHA, S::DlgManualCaptcha);
        set_label(dlg, IDC_BOND, S::DlgBond);
        set_label(dlg, IDOK, S::DlgSave);
        set_label(dlg, IDCANCEL, S::DlgCancel);
        SendDlgItemMessageW(dlg, IDC_LINK, EM_SETLIMITTEXT, kMaxUriLength, 0);
        HICON icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP));
        SendMessageW(dlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
        load_fields(dlg, *ctx);
        apply_theme(dlg, ctx->theme);
        if (!ctx->opts.initial_link.empty()) {
            ctx->loading = true;
            set_utf8(dlg, IDC_LINK, ctx->opts.initial_link);
            ctx->loading = false;
            try_link(dlg, *ctx, true);
        }
        if (ctx->opts.import_mode) {
            SetFocus(GetDlgItem(dlg, IDC_LINK));
            return FALSE;
        }
        return TRUE;
    }
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        return ctx != nullptr ? theme_color(ctx->theme, msg, wp) : 0;
    case WM_COMMAND:
        if (ctx == nullptr) {
            return FALSE;
        }
        switch (LOWORD(wp)) {
        case IDC_LINK:
            // Ссылку вставили прямо в поле - разбираем молча, ошибку покажет OK или Вставить.
            if (HIWORD(wp) == EN_CHANGE && !ctx->loading) {
                try_link(dlg, *ctx, false);
            }
            return TRUE;
        case IDC_OBF:
            // Пейсинг без обфускации ядро не принимает.
            if (HIWORD(wp) == CBN_SELCHANGE && !ctx->loading) {
                const bool obf = combo_value(dlg, IDC_OBF, kObf) != kObfNone;
                if (obf != ctx->obf) {
                    SetDlgItemInt(dlg, IDC_OBF_TIMING, obf ? kDefaultObfTimingMs : 0, FALSE);
                    ctx->obf = obf;
                }
            }
            return TRUE;
        case IDC_PASTE:
            if (auto text = clipboard_text(dlg); text && text->find("freeturn://") != std::string::npos) {
                ctx->loading = true;
                set_utf8(dlg, IDC_LINK, *text);
                ctx->loading = false;
                try_link(dlg, *ctx, true);
            } else {
                message_box(dlg, tr(S::ClipboardEmpty), MB_ICONINFORMATION);
            }
            return TRUE;
        case IDC_FROM_FILE:
            import_file(dlg, *ctx);
            return TRUE;
        case IDOK:
            if (accept_profile(dlg, *ctx)) {
                EndDialog(dlg, IDOK);
            }
            return TRUE;
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        default:
            return FALSE;
        }
    default:
        return FALSE;
    }
}

struct SettingsCtx {
    Settings s;
    std::filesystem::path data_dir;
    std::filesystem::path log_file;
    Theme theme;
};

constexpr const wchar_t* kReleasesUrl = L"https://github.com/toffeehpa/turn-proxy-windows/releases";

struct AboutCtx {
    Theme theme;
    HICON logo = nullptr;
};

INT_PTR CALLBACK about_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    auto* ctx = reinterpret_cast<AboutCtx*>(GetWindowLongPtrW(dlg, DWLP_USER));
    switch (msg) {
    case WM_INITDIALOG: {
        ctx = reinterpret_cast<AboutCtx*>(lp);
        SetWindowLongPtrW(dlg, DWLP_USER, lp);
        SetWindowTextW(dlg, tr(S::AboutTitle));
        const int size = MulDiv(48, static_cast<int>(GetDpiForWindow(dlg)), 96);
        if (SUCCEEDED(LoadIconWithScaleDown(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP), size, size,
                                            &ctx->logo))) {
            SendDlgItemMessageW(dlg, IDC_ABOUT_LOGO, STM_SETICON, reinterpret_cast<WPARAM>(ctx->logo), 0);
        }
        SetDlgItemTextW(dlg, IDC_ABOUT_APP, trf(S::AboutApp, {win::widen(kAppVersion)}).c_str());
        SetDlgItemTextW(dlg, IDC_ABOUT_CORE, trf(S::AboutCore, {win::widen(kCoreVersion)}).c_str());
        SetDlgItemTextW(dlg, IDC_ABOUT_WINTUN, L"Wintun - Copyright (C) 2018-2021 WireGuard LLC");
        set_label(dlg, IDC_ABOUT_UPDATES, S::MenuCheckUpdates);
        set_label(dlg, IDOK, S::DlgClose);
        apply_theme(dlg, ctx->theme);
        center_on_owner(dlg);
        return TRUE;
    }
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        return ctx != nullptr ? theme_color(ctx->theme, msg, wp) : 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        // Автообновления пока нет: проверка - страница релизов в браузере.
        case IDC_ABOUT_UPDATES:
            ShellExecuteW(dlg, L"open", kReleasesUrl, nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        case IDOK:
        case IDCANCEL:
            EndDialog(dlg, IDOK);
            return TRUE;
        default:
            return FALSE;
        }
    case WM_DESTROY:
        if (ctx != nullptr && ctx->logo != nullptr) {
            DestroyIcon(ctx->logo);
            ctx->logo = nullptr;
        }
        return FALSE;
    default:
        return FALSE;
    }
}

void about_dialog(HWND owner) {
    AboutCtx ctx;
    DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_ABOUT), owner, about_proc,
                    reinterpret_cast<LPARAM>(&ctx));
}

INT_PTR CALLBACK settings_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    auto* ctx = reinterpret_cast<SettingsCtx*>(GetWindowLongPtrW(dlg, DWLP_USER));
    switch (msg) {
    case WM_INITDIALOG: {
        ctx = reinterpret_cast<SettingsCtx*>(lp);
        SetWindowLongPtrW(dlg, DWLP_USER, lp);
        SetWindowTextW(dlg, tr(S::DlgSettingsTitle));
        set_label(dlg, IDC_GRP_STARTUP, S::DlgGroupStartup);
        set_label(dlg, IDC_GRP_LOG, S::DlgGroupLog);
        set_label(dlg, IDC_GRP_EXPERIMENTAL, S::DlgGroupExperimental);
        set_label(dlg, IDC_CONNECT_ON_START, S::DlgConnectOnStart);
        set_label(dlg, IDC_AUTO_RECONNECT, S::DlgAutoReconnect);
        set_label(dlg, IDC_VERBOSE_LOG, S::DlgVerboseLog);
        set_label(dlg, IDC_EXPERIMENTAL_SFU, S::DlgExperimentalSfu);
        set_label(dlg, IDC_OPEN_LOG, S::MenuOpenLog);
        set_label(dlg, IDC_OPEN_DATA, S::DlgOpenData);
        set_label(dlg, IDC_ABOUT, S::MenuAbout);
        set_label(dlg, IDOK, S::DlgSave);
        set_label(dlg, IDCANCEL, S::DlgCancel);
        HICON icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP));
        SendMessageW(dlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
        check(dlg, IDC_CONNECT_ON_START, ctx->s.connect_on_start);
        check(dlg, IDC_AUTO_RECONNECT, ctx->s.auto_reconnect);
        check(dlg, IDC_VERBOSE_LOG, ctx->s.verbose_core_log);
        check(dlg, IDC_EXPERIMENTAL_SFU, ctx->s.experimental_sfu);
        apply_theme(dlg, ctx->theme);
        return TRUE;
    }
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        return ctx != nullptr ? theme_color(ctx->theme, msg, wp) : 0;
    case WM_COMMAND:
        if (ctx == nullptr) {
            return FALSE;
        }
        switch (LOWORD(wp)) {
        case IDC_OPEN_LOG:
            ShellExecuteW(dlg, L"open", ctx->log_file.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        case IDC_OPEN_DATA:
            ShellExecuteW(dlg, L"open", ctx->data_dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        case IDC_ABOUT:
            about_dialog(dlg);
            return TRUE;
        case IDOK:
            ctx->s.connect_on_start = checked(dlg, IDC_CONNECT_ON_START);
            ctx->s.auto_reconnect = checked(dlg, IDC_AUTO_RECONNECT);
            ctx->s.verbose_core_log = checked(dlg, IDC_VERBOSE_LOG);
            ctx->s.experimental_sfu = checked(dlg, IDC_EXPERIMENTAL_SFU);
            EndDialog(dlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        default:
            return FALSE;
        }
    default:
        return FALSE;
    }
}

} // namespace

bool profile_dialog(HINSTANCE inst, HWND owner, Profile& p, const ProfileDialogOptions& opts) {
    ProfileCtx ctx;
    ctx.p = p;
    ctx.opts = opts;
    if (DialogBoxParamW(inst, MAKEINTRESOURCEW(IDD_PROFILE), owner, profile_proc,
                        reinterpret_cast<LPARAM>(&ctx)) != IDOK) {
        return false;
    }
    p = std::move(ctx.p);
    return true;
}

bool settings_dialog(HINSTANCE inst, HWND owner, Settings& s, const std::filesystem::path& data_dir,
                     const std::filesystem::path& log_file) {
    SettingsCtx ctx;
    ctx.s = s;
    ctx.data_dir = data_dir;
    ctx.log_file = log_file;
    if (DialogBoxParamW(inst, MAKEINTRESOURCEW(IDD_SETTINGS), owner, settings_proc,
                        reinterpret_cast<LPARAM>(&ctx)) != IDOK) {
        return false;
    }
    s = std::move(ctx.s);
    return true;
}

int message_box(HWND owner, const std::wstring& text, UINT flags) {
    return MessageBoxW(owner, text.c_str(), tr(S::AppName), flags | MB_SETFOREGROUND);
}

} // namespace ft::ui
