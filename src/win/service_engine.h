#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "core/engine.h"

namespace ft::win {

// IEngine для FreeTurn.exe: ядро запускает FreeTurnService, сюда приходят его события.
// Пайп живёт ровно одну сессию - закрыли его, и служба гасит ядро.
class ServiceEngine : public IEngine {
public:
    using Sink = std::function<void(EngineEvent)>;

    ServiceEngine(std::wstring pipe_name, std::wstring server_sid, Sink sink);
    ~ServiceEngine() override;
    ServiceEngine(const ServiceEngine&) = delete;
    ServiceEngine& operator=(const ServiceEngine&) = delete;

    std::expected<void, std::string> start(SessionId session, const EngineLaunch& launch) override;
    void stop() override;
    void shutdown();

private:
    std::wstring pipe_name_;
    std::wstring server_sid_;
    Sink sink_;
    std::mutex mu_;
    void* pipe_ = nullptr;
    std::atomic<bool> running_ = false;
    std::jthread reader_;
};

} // namespace ft::win
