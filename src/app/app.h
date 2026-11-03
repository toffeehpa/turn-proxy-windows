#pragma once

#include <windows.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "app/tray.h"
#include "core/connection_manager.h"
#include "core/ipc_protocol.h"
#include "core/logger.h"
#include "core/profile_store.h"
#include "core/settings.h"
#include "win/dpapi.h"
#include "win/pipe_ipc.h"
#include "win/service_engine.h"

namespace ft::ui {

class WinReconnectTimer : public IReconnectTimer {
public:
    static constexpr UINT_PTR kId = 1;
    void attach(HWND hwnd) { hwnd_ = hwnd; }
    void schedule(std::uint32_t delay_ms) override;
    void cancel() override;

private:
    HWND hwnd_ = nullptr;
};

// Один UI-поток: окно сообщений, трей, ConnectionManager. Остальные потоки только шлют сюда PostMessage.
class App {
public:
    int run(HINSTANCE inst, const std::vector<std::string>& args);

private:
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);

    bool init();
    void load_settings();
    void save_settings();
    void reload_profiles(bool report_broken);
    const Profile* selected() const;
    const Profile* find_profile(std::string_view id_or_name) const;
    std::vector<std::string> profile_names() const;
    void apply_options();

    void on_change(const ConnectionSnapshot& snap);
    void update_tray(const ConnectionSnapshot& snap);
    void show_menu(POINT pt);
    void on_command(UINT id);
    void handle_ipc(const IpcCommand& cmd);

    void connect_profile(const Profile& p);
    void select_profile(const std::string& id);
    void import_profile(const std::string& link);
    void edit_selected();
    void clone_selected();
    void delete_selected();
    void open_settings();
    void request_exit();
    void set_clipboard(const std::wstring& text, bool sensitive);

    HINSTANCE inst_ = nullptr;
    HWND hwnd_ = nullptr;
    UINT taskbar_created_ = 0;
    std::filesystem::path data_dir_;
    Settings settings_;
    Logger logger_;
    win::DpapiProtector protector_;
    std::unique_ptr<ProfileStore> store_;
    std::vector<Profile> profiles_;
    std::vector<std::string> broken_;
    WinReconnectTimer timer_;
    // Порядок важен: manager_ ссылается на engine_ и должен умереть раньше.
    std::unique_ptr<win::ServiceEngine> engine_;
    std::unique_ptr<ConnectionManager> manager_;
    win::PipeServer pipe_;
    TrayIcon tray_;
    HICON icon_off_ = nullptr;
    HICON icon_connecting_ = nullptr;
    HICON icon_on_ = nullptr;
    ConnectionState last_state_ = ConnectionState::Disconnected;
    bool quitting_ = false;
    std::vector<std::string> start_args_;
};

} // namespace ft::ui
