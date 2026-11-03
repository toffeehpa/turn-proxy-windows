#include "win/pipe_ipc.h"

#include <windows.h>
#include <sddl.h>

#include <stop_token>
#include <vector>

#include "core/ipc_protocol.h"
#include "win/handle.h"
#include "win/single_instance.h"

namespace ft::win {

namespace {

constexpr DWORD kBufSize = static_cast<DWORD>(kMaxIpcMessage);

UniqueHandle create_instance(const std::wstring& name) {
    const std::wstring sid = current_user_sid();
    if (sid.empty()) {
        return {};
    }
    // Доступ только своему пользователю и SYSTEM; DACL по умолчанию даёт чтение Everyone.
    const std::wstring sddl = L"D:P(A;;GA;;;" + sid + L")(A;;GA;;;SY)";
    PSECURITY_DESCRIPTOR raw_sd = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &raw_sd,
                                                              nullptr)) {
        return {};
    }
    LocalPtr<void> sd(raw_sd);
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = sd.get();
    // FIRST_PIPE_INSTANCE: имя нельзя занять заранее и подсунуть свой сервер.
    return make_handle(CreateNamedPipeW(
        name.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 0, kBufSize, 0,
        &sa));
}

} // namespace

std::wstring process_user_sid(unsigned long pid) {
    UniqueHandle process = make_handle(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    HANDLE raw_token = nullptr;
    if (!process || !OpenProcessToken(process.get(), TOKEN_QUERY, &raw_token)) {
        return {};
    }
    UniqueHandle token = make_handle(raw_token);
    DWORD size = 0;
    GetTokenInformation(token.get(), TokenUser, nullptr, 0, &size);
    std::vector<BYTE> buf(size);
    LPWSTR raw_str = nullptr;
    if (size == 0 || !GetTokenInformation(token.get(), TokenUser, buf.data(), size, &size) ||
        !ConvertSidToStringSidW(reinterpret_cast<const TOKEN_USER*>(buf.data())->User.Sid, &raw_str)) {
        return {};
    }
    LocalPtr<wchar_t> str(raw_str);
    return str.get();
}

bool write_pipe_message(void* pipe, std::string_view data, unsigned timeout_ms) {
    UniqueHandle ev = make_handle(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    OVERLAPPED ov{};
    ov.hEvent = ev.get();
    DWORD n = 0;
    if (!WriteFile(pipe, data.data(), static_cast<DWORD>(data.size()), nullptr, &ov)) {
        if (GetLastError() != ERROR_IO_PENDING) {
            return false;
        }
        if (WaitForSingleObject(ev.get(), timeout_ms) != WAIT_OBJECT_0) {
            CancelIoEx(pipe, &ov);
            GetOverlappedResult(pipe, &ov, &n, TRUE);
            return false;
        }
    }
    return GetOverlappedResult(pipe, &ov, &n, FALSE) && n == data.size();
}

PipeRead read_pipe_message(void* pipe, void* stop_event, std::size_t max_size, std::string& out) {
    UniqueHandle ev = make_handle(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    OVERLAPPED ov{};
    ov.hEvent = ev.get();
    out.resize(max_size);
    DWORD n = 0;
    if (!ReadFile(pipe, out.data(), static_cast<DWORD>(max_size), nullptr, &ov) &&
        GetLastError() != ERROR_IO_PENDING) {
        out.clear();
        return PipeRead::Broken;
    }
    HANDLE waits[2] = {stop_event, ev.get()};
    if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) != WAIT_OBJECT_0 + 1) {
        CancelIoEx(pipe, &ov);
        GetOverlappedResult(pipe, &ov, &n, TRUE);
        out.clear();
        return PipeRead::Stopped;
    }
    // ERROR_MORE_DATA - сообщение больше лимита: такого клиента не дочитываем, а отключаем.
    const BOOL ok = GetOverlappedResult(pipe, &ov, &n, FALSE);
    out.resize(ok ? n : 0);
    return ok ? PipeRead::Message : PipeRead::Broken;
}

namespace {

bool wait_io(HANDLE pipe, OVERLAPPED& ov, HANDLE stop_event, DWORD timeout_ms, DWORD& transferred) {
    HANDLE waits[2] = {stop_event, ov.hEvent};
    if (WaitForMultipleObjects(2, waits, FALSE, timeout_ms) == WAIT_OBJECT_0 + 1) {
        return GetOverlappedResult(pipe, &ov, &transferred, FALSE) != FALSE;
    }
    CancelIoEx(pipe, &ov);
    GetOverlappedResult(pipe, &ov, &transferred, TRUE);
    return false;
}

void serve(std::stop_token stop, UniqueHandle pipe, const PipeServer::Handler& handler) {
    UniqueHandle stop_event = make_handle(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    UniqueHandle io_event = make_handle(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    std::stop_callback on_stop(stop, [&] { SetEvent(stop_event.get()); });
    std::vector<char> buf(kBufSize);

    while (!stop.stop_requested()) {
        OVERLAPPED ov{};
        ov.hEvent = io_event.get();
        ResetEvent(io_event.get());
        DWORD n = 0;
        bool connected = ConnectNamedPipe(pipe.get(), &ov) != FALSE;
        const DWORD err = GetLastError();
        if (!connected && err == ERROR_IO_PENDING) {
            connected = wait_io(pipe.get(), ov, stop_event.get(), INFINITE, n);
        } else if (!connected) {
            // Клиент мог успеть записать и закрыться до ConnectNamedPipe - сообщение всё ещё в канале.
            connected = err == ERROR_PIPE_CONNECTED || err == ERROR_NO_DATA;
        }
        if (connected) {
            ResetEvent(io_event.get());
            DWORD read = 0;
            bool ok = ReadFile(pipe.get(), buf.data(), kBufSize, &read, &ov) != FALSE;
            if (!ok && GetLastError() == ERROR_IO_PENDING) {
                ok = wait_io(pipe.get(), ov, stop_event.get(), 2000, read);
            }
            if (ok && read > 0 && handler) {
                handler(std::string(buf.data(), read));
            }
        }
        DisconnectNamedPipe(pipe.get());
    }
}

} // namespace

std::wstring default_pipe_name() {
    return L"\\\\.\\pipe\\FreeTurn-" + current_user_sid();
}

bool PipeServer::start(const std::wstring& name, Handler handler) {
    UniqueHandle pipe = create_instance(name);
    if (!pipe) {
        return false;
    }
    thread_ = std::jthread([pipe = std::move(pipe), handler = std::move(handler)](
                               std::stop_token st) mutable { serve(st, std::move(pipe), handler); });
    return true;
}

void PipeServer::stop() {
    thread_.request_stop();
    if (thread_.joinable()) {
        thread_.join();
    }
}

PipeSendResult send_to_pipe(const std::wstring& name, std::string_view message, unsigned timeout_ms) {
    if (message.size() > kMaxIpcMessage) {
        return PipeSendResult::Failed;
    }
    if (!WaitNamedPipeW(name.c_str(), timeout_ms)) {
        return GetLastError() == ERROR_FILE_NOT_FOUND ? PipeSendResult::NotRunning : PipeSendResult::Failed;
    }
    UniqueHandle h = make_handle(CreateFileW(name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                             SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr));
    if (!h) {
        return GetLastError() == ERROR_FILE_NOT_FOUND ? PipeSendResult::NotRunning : PipeSendResult::Failed;
    }
    // Ссылка может нести ключ: отдаём её только серверу, запущенному тем же пользователем.
    ULONG server_pid = 0;
    const std::wstring own = current_user_sid();
    if (!GetNamedPipeServerProcessId(h.get(), &server_pid) || own.empty() ||
        process_user_sid(server_pid) != own) {
        return PipeSendResult::Untrusted;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(h.get(), message.data(), static_cast<DWORD>(message.size()), &written, nullptr);
    return ok && written == message.size() ? PipeSendResult::Sent : PipeSendResult::Failed;
}

} // namespace ft::win
