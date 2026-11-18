#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <thread>

namespace ft::win {

// \\.\pipe\FreeTurn-<SID>: имя на пользователя, пайпы видны всем сеансам машины.
std::wstring default_pipe_name();

// Сервер Named Pipe: читает одно сообщение на подключение и отдаёт его в callback
// из своего потока. Callback сам переносит работу на UI-поток.
class PipeServer {
public:
    using Handler = std::function<void(std::string message)>;

    PipeServer() = default;
    ~PipeServer() { stop(); }
    PipeServer(const PipeServer&) = delete;
    PipeServer& operator=(const PipeServer&) = delete;

    // false - пайп уже занят (другой экземпляр или кто-то чужой).
    bool start(const std::wstring& name, Handler handler);
    void stop();

private:
    std::jthread thread_;
};

enum class PipeSendResult { Sent, NotRunning, Untrusted, Failed };

// Клиент: отправить сообщение первому экземпляру. Перед отправкой проверяет,
// что сервер пайпа запущен тем же пользователем - ссылка может нести ключ.
PipeSendResult send_to_pipe(const std::wstring& name, std::string_view message, unsigned timeout_ms = 2000);

// Общие примитивы для пайпов, открытых с FILE_FLAG_OVERLAPPED.
std::wstring process_user_sid(unsigned long pid);
bool write_pipe_message(void* pipe, std::string_view data, unsigned timeout_ms = 5000);

enum class PipeRead { Message, Stopped, Broken };
PipeRead read_pipe_message(void* pipe, void* stop_event, std::size_t max_size, std::string& out);

} // namespace ft::win
