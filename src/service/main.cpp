#include <windows.h>

#include <cstdio>
#include <format>
#include <memory>

#include "core/logger.h"
#include "core/version.h"
#include "win/handle.h"
#include "win/paths.h"
#include "win/service_host.h"
#include "win/service_names.h"

namespace {

SERVICE_STATUS_HANDLE g_status = nullptr;
ft::win::UniqueHandle g_stop;
ft::Logger g_log;

void report(DWORD state, DWORD exit_code = NO_ERROR) {
    SERVICE_STATUS st{};
    st.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    st.dwCurrentState = state;
    st.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
    st.dwWin32ExitCode = exit_code;
    st.dwWaitHint = state == SERVICE_STOP_PENDING ? ft::win::ProcessEngine::kStopTimeoutMs + 3000 : 3000;
    SetServiceStatus(g_status, &st);
}

DWORD WINAPI on_control(DWORD control, DWORD, void*, void*) {
    if (control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN) {
        report(SERVICE_STOP_PENDING);
        SetEvent(g_stop.get());
        return NO_ERROR;
    }
    return control == SERVICE_CONTROL_INTERROGATE ? NO_ERROR : ERROR_CALL_NOT_IMPLEMENTED;
}

ft::win::ServiceHostOptions host_options() {
    const auto data = ft::win::program_data_dir();
    ft::win::ServiceHostOptions o;
    o.pipe_name = ft::win::kServicePipe;
    o.sddl = ft::win::kServicePipeSddl;
    // Ядро только рядом со службой в Program Files: путь от клиента служба от SYSTEM не принимает.
    o.core_path = ft::win::exe_dir() / ft::win::kCoreExeName;
    o.work_dir = data / L"core";
    o.log = [](ft::LogLevel level, std::string_view text) {
        g_log.write(level, text);
    };
    return o;
}

int run_host() {
    g_log.open(ft::win::program_data_dir() / L"logs" / L"service.log");
    g_log.write(ft::LogLevel::Info, std::format("FreeTurnService {} start", ft::kAppVersion));
    ft::win::ServiceHost host(host_options());
    if (!host.start()) {
        return 1;
    }
    WaitForSingleObject(g_stop.get(), INFINITE);
    host.stop();
    g_log.write(ft::LogLevel::Info, "FreeTurnService stop");
    return 0;
}

void WINAPI service_main(DWORD, LPWSTR*) {
    g_status = RegisterServiceCtrlHandlerExW(ft::win::kServiceName, on_control, nullptr);
    if (g_status == nullptr) {
        return;
    }
    report(SERVICE_START_PENDING);
    g_stop = ft::win::make_handle(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    report(SERVICE_RUNNING);
    const int code = run_host();
    report(SERVICE_STOPPED, code == 0 ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR);
}

BOOL WINAPI on_console_ctrl(DWORD) {
    SetEvent(g_stop.get());
    return TRUE;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    // --console: тот же хост без SCM, для отладки из консоли администратора.
    if (argc > 1 && std::wstring_view(argv[1]) == L"--console") {
        g_stop = ft::win::make_handle(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        SetConsoleCtrlHandler(on_console_ctrl, TRUE);
        std::puts("FreeTurnService in console mode, Ctrl+C to stop");
        return run_host();
    }
    SERVICE_TABLE_ENTRYW table[] = {{const_cast<LPWSTR>(ft::win::kServiceName), service_main},
                                    {nullptr, nullptr}};
    return StartServiceCtrlDispatcherW(table) ? 0 : static_cast<int>(GetLastError());
}
