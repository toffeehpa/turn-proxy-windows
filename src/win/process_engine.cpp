#include "win/process_engine.h"

#include <format>
#include <mutex>
#include <stop_token>
#include <string>
#include <vector>

#include "core/core_args.h"
#include "win/handle.h"
#include "win/wstr.h"

namespace ft::win {

namespace {

constexpr std::size_t kMaxLine = 64 * 1024;

std::mutex g_console_mu;

struct CoreProcess {
    UniqueHandle process;
    UniqueHandle job;
    UniqueHandle read_pipe;
    DWORD pid = 0;
};

std::string win_error(std::string_view what) {
    return std::format("{}: {}", what, last_error_text(GetLastError()));
}

void read_lines(HANDLE pipe, SessionId session, const ProcessEngine::Sink& sink) {
    std::string pending;
    std::vector<char> buf(8192);
    for (;;) {
        DWORD n = 0;
        if (!ReadFile(pipe, buf.data(), static_cast<DWORD>(buf.size()), &n, nullptr) || n == 0) {
            break;
        }
        pending.append(buf.data(), n);
        std::size_t start = 0;
        for (auto nl = pending.find('\n'); nl != std::string::npos; nl = pending.find('\n', start)) {
            sink({EngineEvent::Kind::Output, session, pending.substr(start, nl - start), 0});
            start = nl + 1;
        }
        pending.erase(0, start);
        if (pending.size() > kMaxLine) {
            sink({EngineEvent::Kind::Output, session, pending.substr(0, kMaxLine), 0});
            pending.clear();
        }
    }
    if (!pending.empty()) {
        sink({EngineEvent::Kind::Output, session, pending, 0});
    }
}

void monitor(std::stop_token stop, SessionId session, CoreProcess core, const ProcessEngine::Sink& sink,
             const std::atomic<unsigned>& stop_timeout_ms, std::atomic<bool>& running) {
    std::jthread reader([&] { read_lines(core.read_pipe.get(), session, sink); });

    UniqueHandle stop_event = make_handle(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    std::stop_callback on_stop(stop, [&] { SetEvent(stop_event.get()); });

    HANDLE waits[2] = {core.process.get(), stop_event.get()};
    if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) == WAIT_OBJECT_0 + 1) {
        send_ctrl_break(core.pid);
        if (WaitForSingleObject(core.process.get(), stop_timeout_ms.load()) != WAIT_OBJECT_0) {
            TerminateJobObject(core.job.get(), 1);
            WaitForSingleObject(core.process.get(), 2000);
        }
    }
    // Хвост лога дочитываем, но не ждём вечно: наследник ядра мог унести пайп.
    if (WaitForSingleObject(reader.native_handle(), 2000) != WAIT_OBJECT_0) {
        CancelSynchronousIo(reader.native_handle());
    }
    reader.join();

    DWORD code = 0;
    GetExitCodeProcess(core.process.get(), &code);
    core = {};
    running = false;
    sink({EngineEvent::Kind::Exited, session, {}, static_cast<int>(code)});
}

} // namespace

bool send_ctrl_break(unsigned long pid) {
    std::lock_guard lock(g_console_mu);
    // Событие уходит только в консоль вызывающего, а у ядра своя скрытая - подключаемся к ней.
    const bool had_console = GetConsoleWindow() != nullptr;
    FreeConsole();
    BOOL ok = AttachConsole(pid);
    if (ok) {
        ok = GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid);
        FreeConsole();
    }
    if (had_console) {
        AttachConsole(ATTACH_PARENT_PROCESS);
    }
    return ok != FALSE;
}

ProcessEngine::ProcessEngine(std::function<std::filesystem::path()> core_path, std::filesystem::path work_dir,
                             Sink sink)
    : core_path_(std::move(core_path)), work_dir_(std::move(work_dir)), sink_(std::move(sink)) {}

ProcessEngine::~ProcessEngine() {
    shutdown(kStopTimeoutMs);
}

std::expected<void, std::string> ProcessEngine::start(SessionId session, const EngineLaunch& launch) {
    if (running_) {
        return std::unexpected("freeturn-core.exe is already running");
    }
    if (monitor_.joinable()) {
        monitor_.join();
    }

    const auto exe = core_path_();
    std::error_code ec;
    if (exe.empty() || !std::filesystem::is_regular_file(exe, ec)) {
        return std::unexpected("freeturn-core.exe not found: " + exe.string());
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE read_raw = nullptr;
    HANDLE write_raw = nullptr;
    if (!CreatePipe(&read_raw, &write_raw, &sa, 0)) {
        return std::unexpected(win_error("CreatePipe"));
    }
    CoreProcess core;
    core.read_pipe = make_handle(read_raw);
    UniqueHandle write_end = make_handle(write_raw);
    SetHandleInformation(core.read_pipe.get(), HANDLE_FLAG_INHERIT, 0);
    UniqueHandle null_in = make_handle(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                   &sa, OPEN_EXISTING, 0, nullptr));
    if (!null_in) {
        return std::unexpected(win_error("open NUL"));
    }

    HANDLE inherit[2] = {write_end.get(), null_in.get()};
    ProcThreadAttributes attrs(1);
    if (!attrs.ok() || !UpdateProcThreadAttribute(attrs.get(), 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit,
                                                  sizeof(inherit), nullptr, nullptr)) {
        return std::unexpected(win_error("ProcThreadAttribute"));
    }

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = null_in.get();
    si.StartupInfo.hStdOutput = write_end.get();
    si.StartupInfo.hStdError = write_end.get();
    si.lpAttributeList = attrs.get();

    std::vector<std::string> argv{exe.string()};
    argv.insert(argv.end(), launch.args.begin(), launch.args.end());
    std::wstring cmdline = widen(join_windows_command_line(argv));

    core.job = make_handle(CreateJobObjectW(nullptr, nullptr));
    if (!core.job) {
        return std::unexpected(win_error("CreateJobObject"));
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    // KILL_ON_JOB_CLOSE: упал владелец - ядро не останется сиротой. SILENT_BREAKAWAY: браузер
    // для ручной капчи, запущенный ядром, не должен закрываться вместе с ним.
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                              JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK |
                                              JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    if (!SetInformationJobObject(core.job.get(), JobObjectExtendedLimitInformation, &limits,
                                 sizeof(limits))) {
        return std::unexpected(win_error("SetInformationJobObject"));
    }

    std::filesystem::create_directories(work_dir_, ec);
    PROCESS_INFORMATION pi{};
    const DWORD flags = CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP |
                        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT;
    const BOOL created =
        CreateProcessW(exe.c_str(), cmdline.data(), nullptr, nullptr, TRUE, flags, nullptr,
                       work_dir_.empty() ? nullptr : work_dir_.c_str(), &si.StartupInfo, &pi);
    const DWORD create_err = GetLastError();
    SecureZeroMemory(cmdline.data(), cmdline.size() * sizeof(wchar_t));
    if (!created) {
        return std::unexpected("start freeturn-core.exe: " + last_error_text(create_err));
    }
    core.process = make_handle(pi.hProcess);
    UniqueHandle thread = make_handle(pi.hThread);
    core.pid = pi.dwProcessId;
    if (!AssignProcessToJobObject(core.job.get(), core.process.get())) {
        const auto err = win_error("AssignProcessToJobObject");
        TerminateProcess(core.process.get(), 1);
        return std::unexpected(err);
    }
    ResumeThread(thread.get());
    // Пишущий конец держит только ядро - иначе ReadFile не дождётся EOF.
    write_end.reset();

    running_ = true;
    monitor_ = std::jthread([this, session, core = std::move(core)](std::stop_token st) mutable {
        monitor(st, session, std::move(core), sink_, stop_timeout_ms_, running_);
    });
    return {};
}

void ProcessEngine::stop() {
    monitor_.request_stop();
}

void ProcessEngine::shutdown(unsigned timeout_ms) {
    stop_timeout_ms_ = timeout_ms;
    monitor_.request_stop();
    if (monitor_.joinable()) {
        monitor_.join();
    }
}

} // namespace ft::win
