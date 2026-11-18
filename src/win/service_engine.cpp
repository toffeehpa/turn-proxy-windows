#include "win/service_engine.h"

#include <windows.h>

#include <stop_token>

#include "core/service_protocol.h"
#include "win/handle.h"
#include "win/pipe_ipc.h"

namespace ft::win {

ServiceEngine::ServiceEngine(std::wstring pipe_name, std::wstring server_sid, Sink sink)
    : pipe_name_(std::move(pipe_name)), server_sid_(std::move(server_sid)), sink_(std::move(sink)) {}

ServiceEngine::~ServiceEngine() {
    shutdown();
}

std::expected<void, std::string> ServiceEngine::start(SessionId session, const EngineLaunch& launch) {
    if (running_) {
        return std::unexpected("freeturn-core.exe is already running");
    }
    if (reader_.joinable()) {
        reader_.join();
    }
    if (!WaitNamedPipeW(pipe_name_.c_str(), 3000)) {
        return std::unexpected("FreeTurn service is not running");
    }
    UniqueHandle pipe = make_handle(
        CreateFileW(pipe_name_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                    FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr));
    if (!pipe) {
        return std::unexpected("FreeTurn service is not available");
    }
    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(pipe.get(), &mode, nullptr, nullptr);
    // Профиль с ключами отдаём только настоящей службе: пайп с этим именем мог создать кто угодно.
    ULONG server_pid = 0;
    if (!GetNamedPipeServerProcessId(pipe.get(), &server_pid) ||
        process_user_sid(server_pid) != server_sid_) {
        return std::unexpected("FreeTurn service pipe is owned by an untrusted process");
    }
    if (!write_pipe_message(pipe.get(), encode_request({ServiceRequest::Kind::Start, launch.profile}))) {
        return std::unexpected("FreeTurn service did not accept the request");
    }

    {
        std::lock_guard lock(mu_);
        pipe_ = pipe.get();
    }
    running_ = true;
    reader_ = std::jthread([this, session, owned = std::move(pipe)](std::stop_token st) {
        UniqueHandle stop_event = make_handle(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        std::stop_callback on_stop(st, [&] { SetEvent(stop_event.get()); });
        std::string msg;
        bool exited = false;
        int code = -1;
        while (!exited) {
            const auto r = read_pipe_message(owned.get(), stop_event.get(), kMaxServiceMessage, msg);
            if (r == PipeRead::Stopped) {
                break;
            }
            if (r == PipeRead::Broken) {
                sink_({EngineEvent::Kind::Output, session, "[ERROR] connection to FreeTurn service lost", 0});
                exited = true;
                break;
            }
            const auto ev = decode_event(msg);
            if (!ev) {
                continue;
            }
            switch (ev->kind) {
            case ServiceEvent::Kind::Output:
                sink_({EngineEvent::Kind::Output, session, ev->text, 0});
                break;
            case ServiceEvent::Kind::Error:
                sink_({EngineEvent::Kind::Output, session, "[ERROR] " + ev->text, 0});
                exited = true;
                break;
            case ServiceEvent::Kind::Exited:
                code = ev->code;
                exited = true;
                break;
            }
        }
        {
            std::lock_guard lock(mu_);
            pipe_ = nullptr;
        }
        running_ = false;
        if (exited) {
            sink_({EngineEvent::Kind::Exited, session, {}, code});
        }
    });
    return {};
}

void ServiceEngine::stop() {
    std::lock_guard lock(mu_);
    if (pipe_ != nullptr) {
        write_pipe_message(pipe_, encode_request({ServiceRequest::Kind::Stop, {}}));
    }
}

void ServiceEngine::shutdown() {
    // Без ответа службы: закрытый пайп сам остановит ядро на её стороне.
    reader_.request_stop();
    if (reader_.joinable()) {
        reader_.join();
    }
}

} // namespace ft::win
