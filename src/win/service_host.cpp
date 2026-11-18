#include "win/service_host.h"

#include <windows.h>
#include <sddl.h>

#include <format>

#include "core/core_args.h"
#include "core/service_protocol.h"
#include "win/handle.h"
#include "win/pipe_ipc.h"
#include "win/wstr.h"

namespace ft::win {

namespace {

UniqueHandle create_service_pipe(const std::wstring& name, const std::wstring& sddl) {
    PSECURITY_DESCRIPTOR raw_sd = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &raw_sd,
                                                              nullptr)) {
        return {};
    }
    LocalPtr<void> sd(raw_sd);
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = sd.get();
    const auto max = static_cast<DWORD>(kMaxServiceMessage);
    return make_handle(CreateNamedPipeW(
        name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, max, max, 0,
        &sa));
}

} // namespace

ServiceHost::ServiceHost(ServiceHostOptions opts) : opts_(std::move(opts)) {
    engine_ = std::make_unique<ProcessEngine>(
        [path = opts_.core_path] { return path; }, opts_.work_dir,
        [this](EngineEvent e) {
            if (e.kind == EngineEvent::Kind::Exited) {
                core_running_ = false;
                log(LogLevel::Info, std::format("freeturn-core.exe exited, code={}", e.exit_code));
                send({ServiceEvent::Kind::Exited, {}, e.exit_code});
            } else {
                send({ServiceEvent::Kind::Output, std::move(e.line), 0});
            }
        });
}

ServiceHost::~ServiceHost() {
    stop();
}

void ServiceHost::log(LogLevel level, std::string_view text) const {
    if (opts_.log) {
        opts_.log(level, text);
    }
}

bool ServiceHost::start() {
    UniqueHandle pipe = create_service_pipe(opts_.pipe_name, opts_.sddl);
    if (!pipe) {
        log(LogLevel::Error, "create service pipe: " + last_error_text(GetLastError()));
        return false;
    }
    thread_ = std::jthread([this, p = pipe.release()](std::stop_token st) { serve(st, p); });
    return true;
}

void ServiceHost::stop() {
    thread_.request_stop();
    if (thread_.joinable()) {
        thread_.join();
    }
    engine_->shutdown(ProcessEngine::kStopTimeoutMs);
}

void ServiceHost::send(const ServiceEvent& e) {
    std::lock_guard lock(write_mu_);
    if (client_ != nullptr) {
        write_pipe_message(client_, encode_event(e));
    }
}

void ServiceHost::handle(void* pipe, std::string_view message) {
    auto req = decode_request(message);
    if (!req) {
        log(LogLevel::Warn, "rejected request: " + req.error());
        send({ServiceEvent::Kind::Error, req.error(), 0});
        return;
    }
    if (req->kind == ServiceRequest::Kind::Stop) {
        engine_->stop();
        return;
    }
    if (core_running_) {
        send({ServiceEvent::Kind::Error, "freeturn-core.exe is already running", 0});
        return;
    }
    EngineLaunch launch;
    launch.profile = req->profile;
    launch.args = build_core_args(launch.profile, {.debug = true, .routes = opts_.routes});
    launch.display_args = redact_core_args(launch.args);
    std::string shown;
    for (const auto& a : launch.display_args) {
        shown += ' ';
        shown += a;
    }
    ULONG pid = 0;
    GetNamedPipeClientProcessId(pipe, &pid);
    log(LogLevel::Info, std::format("start for {}:{}", narrow(process_user_sid(pid)), shown));
    core_running_ = true;
    if (auto r = engine_->start(++session_, launch); !r) {
        core_running_ = false;
        log(LogLevel::Error, r.error());
        send({ServiceEvent::Kind::Error, r.error(), 0});
    }
}

void ServiceHost::serve(std::stop_token stop, void* raw_pipe) {
    UniqueHandle pipe(raw_pipe);
    UniqueHandle stop_event = make_handle(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    UniqueHandle io_event = make_handle(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    std::stop_callback on_stop(stop, [&] { SetEvent(stop_event.get()); });

    while (!stop.stop_requested()) {
        OVERLAPPED ov{};
        ov.hEvent = io_event.get();
        ResetEvent(io_event.get());
        bool connected = ConnectNamedPipe(pipe.get(), &ov) != FALSE;
        if (!connected && GetLastError() == ERROR_IO_PENDING) {
            HANDLE waits[2] = {stop_event.get(), io_event.get()};
            DWORD n = 0;
            if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) != WAIT_OBJECT_0 + 1) {
                CancelIoEx(pipe.get(), &ov);
                GetOverlappedResult(pipe.get(), &ov, &n, TRUE);
                break;
            }
            connected = GetOverlappedResult(pipe.get(), &ov, &n, FALSE) != FALSE;
        } else if (!connected) {
            connected = GetLastError() == ERROR_PIPE_CONNECTED;
        }
        if (connected) {
            {
                std::lock_guard lock(write_mu_);
                client_ = pipe.get();
            }
            std::string msg;
            while (read_pipe_message(pipe.get(), stop_event.get(), kMaxServiceMessage, msg) ==
                   PipeRead::Message) {
                handle(pipe.get(), msg);
            }
            // Клиент ушёл или служба останавливается: ядро без хозяина не оставляем.
            engine_->shutdown(ProcessEngine::kStopTimeoutMs);
            std::lock_guard lock(write_mu_);
            client_ = nullptr;
        }
        DisconnectNamedPipe(pipe.get());
    }
}

} // namespace ft::win
