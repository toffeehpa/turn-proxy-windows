#include "app/app.h"

#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <memory>

#include "app/dialogs.h"
#include "app/strings.h"
#include "core/freeturn_uri.h"
#include "core/json.h"
#include "core/version.h"
#include "resource.h"
#include "win/handle.h"
#include "win/paths.h"
#include "win/service_names.h"
#include "win/wstr.h"

namespace ft::ui {

namespace {

constexpr UINT WM_APP_TRAY = WM_APP + 1;
constexpr UINT WM_APP_ENGINE = WM_APP + 2;
constexpr UINT WM_APP_IPC = WM_APP + 3;

constexpr UINT_PTR kQuitTimer = 2;
constexpr UINT kQuitTimeoutMs = 10'000;

enum : UINT {
    ID_CONNECT = 40001,
    ID_DISCONNECT,
    ID_COPY_ADDRESS,
    ID_IMPORT,
    ID_SETTINGS,
    ID_EXIT,
    ID_EDIT,
    ID_CLONE,
    ID_COPY_LINK,
    ID_DELETE,
    ID_PROFILE_BASE = 41000,
    ID_PROFILE_LAST = 41499,
};

constexpr const wchar_t* kWindowClass = L"FreeTurn.Tray";

std::wstring menu_text(std::string_view utf8) {
    std::wstring out;
    for (const wchar_t c : win::widen(utf8)) {
        out.push_back(c);
        if (c == L'&') {
            out.push_back(L'&');
        }
    }
    return out;
}

std::wstring clip(std::wstring s, std::size_t n) {
    if (s.size() > n) {
        s.resize(n - 1);
        s.push_back(L'\x2026');
    }
    return s;
}

HICON load_tray_icon(HINSTANCE inst, int id) {
    HICON icon = nullptr;
    if (FAILED(LoadIconMetric(inst, MAKEINTRESOURCEW(id), LIM_SMALL, &icon))) {
        icon = LoadIconW(inst, MAKEINTRESOURCEW(id));
    }
    return icon;
}

// Автозапуск трея у текущего пользователя: значение в HKCU\...\Run, права администратора не нужны.
LSTATUS set_launch_on_logon(bool on) {
    constexpr const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    constexpr const wchar_t* kValue = L"FreeTurn";
    if (!on) {
        const LSTATUS st = RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kValue);
        return st == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : st;
    }
    std::wstring exe(32768, L'\0');
    exe.resize(GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size())));
    const std::wstring cmd = L"\"" + exe + L"\"";
    return RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kValue, REG_SZ, cmd.c_str(),
                           static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
}

} // namespace

void WinReconnectTimer::schedule(std::uint32_t delay_ms) {
    SetTimer(hwnd_, kId, delay_ms, nullptr);
}

void WinReconnectTimer::cancel() {
    KillTimer(hwnd_, kId);
}

int App::run(HINSTANCE inst, const std::vector<std::string>& args) {
    inst_ = inst;
    start_args_ = args;
    if (!init()) {
        return 1;
    }
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    manager_.reset();
    engine_->shutdown();
    pipe_.stop();
    logger_.write(LogLevel::Info, "exit");
    return 0;
}

bool App::init() {
    data_dir_ = win::app_data_dir();
    std::error_code ec;
    for (const auto* sub : {L"profiles", L"secure", L"logs", L"updates"}) {
        std::filesystem::create_directories(data_dir_ / sub, ec);
    }
    logger_.open(data_dir_ / L"logs" / L"freeturn.log");
    logger_.write(LogLevel::Info, std::format("FreeTurn {} start", kAppVersion));
    load_settings();
    // Путь мог смениться после переустановки - обновляем запись.
    if (settings_.launch_on_logon) {
        sync_launch_on_logon();
    }

    store_ = std::make_unique<ProfileStore>(data_dir_, protector_);
    reload_profiles(false);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &App::wnd_proc;
    wc.hInstance = inst_;
    wc.hIcon = LoadIconW(inst_, MAKEINTRESOURCEW(IDI_APP));
    wc.lpszClassName = kWindowClass;
    if (RegisterClassExW(&wc) == 0) {
        return false;
    }
    // Обычное скрытое окно, не HWND_MESSAGE: broadcast TaskbarCreated до message-only окон не доходит.
    hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClass, L"FreeTurn", WS_OVERLAPPED, 0, 0, 0, 0, nullptr,
                            nullptr, inst_, this);
    if (hwnd_ == nullptr) {
        return false;
    }
    taskbar_created_ = RegisterWindowMessageW(L"TaskbarCreated");

    icon_off_ = load_tray_icon(inst_, IDI_TRAY_OFF);
    icon_connecting_ = load_tray_icon(inst_, IDI_TRAY_CONNECTING);
    icon_on_ = load_tray_icon(inst_, IDI_TRAY_ON);

    timer_.attach(hwnd_);
    HWND hwnd = hwnd_;
    engine_ =
        std::make_unique<win::ServiceEngine>(win::kServicePipe, win::kLocalSystemSid, [hwnd](EngineEvent e) {
            auto* p = new EngineEvent(std::move(e));
            if (!PostMessageW(hwnd, WM_APP_ENGINE, 0, reinterpret_cast<LPARAM>(p))) {
                delete p;
            }
        });
    manager_ = std::make_unique<ConnectionManager>(*engine_, timer_);
    apply_options();
    manager_->on_change([this](const ConnectionSnapshot& s) { on_change(s); });
    manager_->on_log([this](LogLevel level, std::string_view text) { logger_.write(level, text); });

    tray_.add(hwnd_, WM_APP_TRAY, icon_off_, tr(S::AppName));

    const bool pipe_ok = pipe_.start(win::default_pipe_name(), [hwnd](std::string m) {
        auto* p = new std::string(std::move(m));
        if (!PostMessageW(hwnd, WM_APP_IPC, 0, reinterpret_cast<LPARAM>(p))) {
            delete p;
        }
    });
    if (!pipe_ok) {
        logger_.write(LogLevel::Warn, "named pipe is busy, second-instance commands disabled");
    }

    if (!broken_.empty()) {
        reload_profiles(true);
    }
    const IpcCommand first = command_from_args(start_args_);
    if (first.kind != IpcCommand::Kind::Show) {
        handle_ipc(first);
    } else if (settings_.connect_on_start) {
        if (const Profile* p = selected()) {
            connect_profile(*p);
        }
    }
    return true;
}

void App::load_settings() {
    if (auto text = read_file(data_dir_ / L"settings.json", 1024 * 1024)) {
        settings_ = parse_settings(*text);
    }
    logger_.set_debug(settings_.verbose_core_log);
}

void App::save_settings() {
    if (auto r = write_file_atomic(data_dir_ / L"settings.json",
                                   json::serialize(settings_to_json(settings_)) + "\n");
        !r) {
        logger_.write(LogLevel::Error, "save settings: " + r.error());
    }
}

void App::apply_options() {
    ConnectionOptions o;
    o.auto_reconnect = settings_.auto_reconnect;
    o.allow_sfu = settings_.experimental_sfu;
    o.verbose_core_log = settings_.verbose_core_log;
    manager_->set_options(o);
    logger_.set_debug(settings_.verbose_core_log);
}

void App::reload_profiles(bool report_broken) {
    auto loaded = store_->load_all();
    profiles_ = std::move(loaded.profiles);
    broken_ = std::move(loaded.broken);
    for (const auto& b : broken_) {
        logger_.write(LogLevel::Warn, "cannot read profile file " + b);
    }
    if (report_broken && !broken_.empty()) {
        std::wstring list;
        for (const auto& b : broken_) {
            list += (list.empty() ? L"" : L", ") + win::widen(b);
        }
        tray_.balloon(tr(S::AppName), trf(S::BrokenProfiles, {list}), true);
    }
    if (selected() == nullptr) {
        settings_.last_profile_id = profiles_.empty() ? std::string() : profiles_.front().id;
    }
}

const Profile* App::selected() const {
    for (const auto& p : profiles_) {
        if (p.id == settings_.last_profile_id) {
            return &p;
        }
    }
    return nullptr;
}

const Profile* App::find_profile(std::string_view id_or_name) const {
    for (const auto& p : profiles_) {
        if (p.id == id_or_name || p.name == id_or_name) {
            return &p;
        }
    }
    return nullptr;
}

std::vector<std::string> App::profile_names() const {
    std::vector<std::string> names;
    for (const auto& p : profiles_) {
        names.push_back(p.name);
    }
    return names;
}

LRESULT CALLBACK App::wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    }
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (app == nullptr || app->hwnd_ == nullptr) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return app->handle(msg, wp, lp);
}

LRESULT App::handle(UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == taskbar_created_ && taskbar_created_ != 0 && manager_) {
        tray_.remove();
        tray_.add(hwnd_, WM_APP_TRAY, icon_off_, tr(S::AppName));
        update_tray(manager_->snapshot());
        return 0;
    }
    switch (msg) {
    case WM_APP_TRAY:
        switch (LOWORD(lp)) {
        case WM_CONTEXTMENU:
        case NIN_SELECT:
        case NIN_KEYSELECT:
            show_menu(POINT{GET_X_LPARAM(wp), GET_Y_LPARAM(wp)});
            break;
        default:
            break;
        }
        return 0;
    case WM_APP_ENGINE: {
        std::unique_ptr<EngineEvent> ev(reinterpret_cast<EngineEvent*>(lp));
        if (manager_) {
            manager_->on_engine_event(*ev);
        }
        return 0;
    }
    case WM_APP_IPC: {
        std::unique_ptr<std::string> m(reinterpret_cast<std::string*>(lp));
        if (auto cmd = decode_ipc(*m)) {
            handle_ipc(*cmd);
        } else {
            logger_.write(LogLevel::Warn, "ignored malformed pipe message");
        }
        return 0;
    }
    case WM_COMMAND:
        on_command(LOWORD(wp));
        return 0;
    case WM_TIMER:
        if (wp == WinReconnectTimer::kId) {
            KillTimer(hwnd_, WinReconnectTimer::kId);
            manager_->on_reconnect_timer();
        } else if (wp == kQuitTimer) {
            KillTimer(hwnd_, kQuitTimer);
            logger_.write(LogLevel::Warn, "freeturn-core.exe did not stop in time, closing service session");
            engine_->shutdown();
            DestroyWindow(hwnd_);
        }
        return 0;
    case WM_QUERYENDSESSION:
        return TRUE;
    case WM_ENDSESSION:
        if (wp != FALSE) {
            engine_->shutdown();
        }
        return 0;
    case WM_DESTROY:
        tray_.remove();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd_, msg, wp, lp);
    }
}

void App::update_tray(const ConnectionSnapshot& snap) {
    HICON icon = icon_off_;
    switch (snap.state) {
    case ConnectionState::Connected:
        icon = icon_on_;
        break;
    case ConnectionState::Starting:
    case ConnectionState::Connecting:
    case ConnectionState::Captcha:
    case ConnectionState::Reconnecting:
        icon = icon_connecting_;
        break;
    default:
        break;
    }
    std::wstring tip = std::wstring(tr(S::AppName)) + L" - " + state_text(snap.state);
    if (snap.state == ConnectionState::Connected || snap.state == ConnectionState::Connecting) {
        tip += std::format(L"\n{} ({}/{})", win::widen(snap.local_address), snap.active, snap.total);
    } else if (snap.state == ConnectionState::Error && !snap.error.empty()) {
        tip += L"\n" + win::widen(snap.error);
    }
    tray_.update(icon, clip(tip, 127));
}

void App::on_change(const ConnectionSnapshot& snap) {
    update_tray(snap);
    if (snap.state != last_state_) {
        switch (snap.state) {
        case ConnectionState::Connected:
            tray_.balloon(menu_text(snap.profile_name),
                          trf(S::NotifyConnected, {win::widen(snap.local_address)}));
            break;
        case ConnectionState::Captcha:
            tray_.balloon(menu_text(snap.profile_name), tr(S::NotifyCaptcha));
            break;
        case ConnectionState::Reconnecting:
            // attempt 0 - роллинг перешёл к следующему звонку: об этом говорит только цвет в списке.
            if (snap.attempt > 0) {
                tray_.balloon(menu_text(snap.profile_name),
                              trf(S::NotifyReconnecting,
                                  {std::to_wstring(snap.attempt), std::to_wstring(kMaxReconnectAttempts)}));
            }
            break;
        case ConnectionState::Error:
            tray_.balloon(menu_text(snap.profile_name),
                          trf(S::NotifyError, {clip(win::widen(snap.error), 200)}), true);
            break;
        default:
            break;
        }
        last_state_ = snap.state;
    }
    if (quitting_ && !is_busy(snap.state)) {
        KillTimer(hwnd_, kQuitTimer);
        DestroyWindow(hwnd_);
    }
}

// Состояние - в иконке и подсказке трея, в меню из него только потоки.
void App::show_menu(POINT pt) {
    const auto& snap = manager_->snapshot();
    const bool busy = is_busy(snap.state);
    const Profile* sel = selected();
    win::UniqueMenu owned_menu(CreatePopupMenu());
    HMENU menu = owned_menu.get();

    auto add = [](HMENU m, UINT id, const std::wstring& text, bool enabled = true) {
        AppendMenuW(m, MF_STRING | (enabled ? 0 : MF_GRAYED), id, text.c_str());
    };

    if (busy && snap.total > 0 && snap.state != ConnectionState::Stopping) {
        add(menu, 0, trf(S::MenuStreams, {std::to_wstring(snap.active), std::to_wstring(snap.total)}), false);
    }
    if (busy) {
        add(menu, ID_DISCONNECT, tr(S::MenuDisconnect), snap.state != ConnectionState::Stopping);
    } else {
        add(menu, ID_CONNECT, tr(S::MenuConnect), sel != nullptr);
    }

    HMENU list = CreatePopupMenu();
    if (profiles_.empty()) {
        add(list, 0, tr(S::MenuNoProfiles), false);
    }
    for (std::size_t i = 0; i < profiles_.size() && ID_PROFILE_BASE + i <= ID_PROFILE_LAST; ++i) {
        const auto& p = profiles_[i];
        const UINT id = ID_PROFILE_BASE + static_cast<UINT>(i);
        add(list, id, menu_text(p.name));
        if (sel != nullptr && p.id == sel->id) {
            CheckMenuRadioItem(list, id, id, id, MF_BYCOMMAND);
        }
    }
    AppendMenuW(list, MF_SEPARATOR, 0, nullptr);
    add(list, ID_IMPORT, tr(S::MenuImport));
    add(list, ID_EDIT, tr(S::MenuEdit), sel != nullptr);
    add(list, ID_COPY_LINK, tr(S::MenuCopyLink), sel != nullptr);
    add(list, ID_COPY_ADDRESS, tr(S::MenuCopyAddress), busy || sel != nullptr);
    add(list, ID_CLONE, tr(S::MenuClone), sel != nullptr);
    AppendMenuW(list, MF_SEPARATOR, 0, nullptr);
    add(list, ID_DELETE, tr(S::MenuDelete), sel != nullptr);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(list), tr(S::MenuProfiles));

    add(menu, ID_SETTINGS, tr(S::MenuSettings));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    add(menu, ID_EXIT, tr(S::MenuExit));

    show_tray_menu(hwnd_, menu, pt);
}

void App::on_command(UINT id) {
    if (id >= ID_PROFILE_BASE && id <= ID_PROFILE_LAST) {
        const std::size_t i = id - ID_PROFILE_BASE;
        if (i < profiles_.size()) {
            select_profile(profiles_[i].id);
        }
        return;
    }
    const auto& snap = manager_->snapshot();
    switch (id) {
    case ID_CONNECT:
        if (const Profile* p = selected()) {
            connect_profile(*p);
        }
        break;
    case ID_DISCONNECT:
        manager_->disconnect();
        break;
    case ID_COPY_ADDRESS: {
        const Profile* p = selected();
        const std::string addr = is_busy(snap.state) ? snap.local_address : (p != nullptr ? p->listen : "");
        if (!addr.empty()) {
            set_clipboard(win::widen(addr), false);
        }
        break;
    }
    case ID_IMPORT:
        import_profile({});
        break;
    case ID_SETTINGS:
        open_settings();
        break;
    case ID_EXIT:
        request_exit();
        break;
    case ID_EDIT:
        edit_selected();
        break;
    case ID_CLONE:
        clone_selected();
        break;
    case ID_COPY_LINK:
        if (const Profile* p = selected()) {
            set_clipboard(win::widen(build_freeturn_uri(*p)), true);
        }
        break;
    case ID_DELETE:
        delete_selected();
        break;
    default:
        break;
    }
}

void App::handle_ipc(const IpcCommand& cmd) {
    switch (cmd.kind) {
    case IpcCommand::Kind::Show:
        tray_.balloon(tr(S::AppName), tr(S::AlreadyRunning));
        break;
    case IpcCommand::Kind::Import:
        import_profile(cmd.arg);
        break;
    case IpcCommand::Kind::Connect: {
        const Profile* p = cmd.arg.empty() ? selected() : find_profile(cmd.arg);
        if (p != nullptr) {
            settings_.last_profile_id = p->id;
            save_settings();
            connect_profile(*p);
        }
        break;
    }
    case IpcCommand::Kind::Disconnect:
        manager_->disconnect();
        break;
    case IpcCommand::Kind::Reconnect:
        manager_->reconnect();
        break;
    case IpcCommand::Kind::Quit:
        request_exit();
        break;
    }
}

void App::connect_profile(const Profile& p) {
    switch (manager_->connect(p)) {
    case ConnectResult::InvalidProfile: {
        std::wstring list;
        for (const auto i : validate(p)) {
            list += (list.empty() ? L"" : L"; ") + issue_text(i);
        }
        message_box(hwnd_, trf(S::InvalidProfile, {list}), MB_ICONWARNING);
        break;
    }
    case ConnectResult::ExperimentalDisabled:
        message_box(hwnd_, tr(S::ExperimentalDisabled), MB_ICONINFORMATION);
        break;
    default:
        break;
    }
}

void App::select_profile(const std::string& id) {
    settings_.last_profile_id = id;
    save_settings();
    // Выбор другого профиля во время подключения - переключение на него.
    if (is_busy(manager_->state())) {
        if (const Profile* p = selected()) {
            connect_profile(*p);
        }
    }
}

void App::import_profile(const std::string& link) {
    Profile p;
    ProfileDialogOptions opts;
    opts.import_mode = true;
    opts.allow_sfu = settings_.experimental_sfu;
    opts.initial_link = link;
    if (!profile_dialog(inst_, hwnd_, p, opts)) {
        return;
    }
    p.id.clear();
    p.name = unique_name(p.name, profile_names());
    if (auto r = store_->save(p); !r) {
        message_box(hwnd_, trf(S::SaveFailed, {win::widen(r.error())}), MB_ICONERROR);
        return;
    }
    logger_.write(LogLevel::Info, "profile imported: " + p.name);
    settings_.last_profile_id = p.id;
    save_settings();
    reload_profiles(false);
    tray_.balloon(tr(S::AppName), trf(S::NotifyImported, {menu_text(p.name)}));
}

void App::edit_selected() {
    const Profile* sel = selected();
    if (sel == nullptr) {
        return;
    }
    Profile p = *sel;
    ProfileDialogOptions opts;
    opts.allow_sfu = settings_.experimental_sfu;
    if (manager_->snapshot().profile_id == p.id) {
        opts.calls = manager_->snapshot().calls;
    }
    if (!profile_dialog(inst_, hwnd_, p, opts)) {
        return;
    }
    auto names = profile_names();
    std::erase(names, sel->name);
    p.name = unique_name(p.name, names);
    if (auto r = store_->save(p); !r) {
        message_box(hwnd_, trf(S::SaveFailed, {win::widen(r.error())}), MB_ICONERROR);
        return;
    }
    const bool active = manager_->current_profile() && manager_->current_profile()->id == p.id;
    reload_profiles(false);
    if (active && is_busy(manager_->state())) {
        tray_.balloon(menu_text(p.name), tr(S::EditApplyLater));
    }
}

void App::clone_selected() {
    const Profile* sel = selected();
    if (sel == nullptr) {
        return;
    }
    auto copy = store_->clone(*sel, profile_names());
    if (!copy) {
        message_box(hwnd_, trf(S::SaveFailed, {win::widen(copy.error())}), MB_ICONERROR);
        return;
    }
    settings_.last_profile_id = copy->id;
    save_settings();
    reload_profiles(false);
}

void App::delete_selected() {
    const Profile* sel = selected();
    if (sel == nullptr) {
        return;
    }
    if (is_busy(manager_->state()) && manager_->current_profile() &&
        manager_->current_profile()->id == sel->id) {
        message_box(hwnd_, tr(S::DeleteActive), MB_ICONINFORMATION);
        return;
    }
    if (message_box(hwnd_, trf(S::ConfirmDelete, {menu_text(sel->name)}), MB_YESNO | MB_ICONQUESTION) !=
        IDYES) {
        return;
    }
    const std::string id = sel->id;
    if (auto r = store_->remove(id); !r) {
        message_box(hwnd_, trf(S::SaveFailed, {win::widen(r.error())}), MB_ICONERROR);
        return;
    }
    logger_.write(LogLevel::Info, "profile deleted: " + id);
    reload_profiles(false);
    save_settings();
}

void App::open_settings() {
    Settings s = settings_;
    if (!settings_dialog(inst_, hwnd_, s, data_dir_, logger_.path())) {
        return;
    }
    const bool launch_changed = s.launch_on_logon != settings_.launch_on_logon;
    settings_ = s;
    save_settings();
    apply_options();
    if (launch_changed) {
        sync_launch_on_logon();
    }
}

void App::sync_launch_on_logon() {
    if (const LSTATUS st = set_launch_on_logon(settings_.launch_on_logon); st != ERROR_SUCCESS) {
        logger_.write(LogLevel::Warn, std::format("launch on logon: registry error {}", st));
    }
}

void App::request_exit() {
    if (!is_busy(manager_->state())) {
        DestroyWindow(hwnd_);
        return;
    }
    quitting_ = true;
    SetTimer(hwnd_, kQuitTimer, kQuitTimeoutMs, nullptr);
    manager_->disconnect();
}

void App::set_clipboard(const std::wstring& text, bool sensitive) {
    win::ClipboardLock clipboard(hwnd_);
    if (!clipboard.ok()) {
        return;
    }
    EmptyClipboard();
    auto put = [](UINT format, const void* data, std::size_t bytes) {
        win::UniqueGlobal mem(GlobalAlloc(GMEM_MOVEABLE, bytes));
        void* dst = mem ? GlobalLock(mem.get()) : nullptr;
        if (dst == nullptr) {
            return;
        }
        std::memcpy(dst, data, bytes);
        GlobalUnlock(mem.get());
        if (SetClipboardData(format, mem.get()) != nullptr) {
            mem.release();
        }
    };
    put(CF_UNICODETEXT, text.c_str(), (text.size() + 1) * sizeof(wchar_t));
    // Ссылка несёт ключ обфускации: не пускаем её в журнал буфера (Win+V) и облачную синхронизацию.
    if (sensitive) {
        const DWORD no = 0;
        put(RegisterClipboardFormatW(L"CanIncludeInClipboardHistory"), &no, sizeof(no));
        put(RegisterClipboardFormatW(L"CanUploadToCloudClipboard"), &no, sizeof(no));
    }
}

} // namespace ft::ui
