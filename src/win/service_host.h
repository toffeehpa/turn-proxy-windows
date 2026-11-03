#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "core/log_parser.h"
#include "core/service_protocol.h"
#include "win/process_engine.h"

namespace ft::win {

struct ServiceHostOptions {
    std::wstring pipe_name;
    std::wstring sddl;
    std::filesystem::path core_path;
    std::filesystem::path work_dir;
    bool routes = true;
    std::function<void(LogLevel, std::string_view)> log;
};

// Сердце FreeTurnService без SCM: пайп для FreeTurn.exe и ядро. Один клиент за раз;
// клиент отключился - его ядро останавливается, сиротских туннелей не бывает.
class ServiceHost {
public:
    explicit ServiceHost(ServiceHostOptions opts);
    ~ServiceHost();
    ServiceHost(const ServiceHost&) = delete;
    ServiceHost& operator=(const ServiceHost&) = delete;

    bool start();
    void stop();

private:
    void serve(std::stop_token stop, void* pipe);
    void handle(void* pipe, std::string_view message);
    void send(const ServiceEvent& e);
    void log(LogLevel level, std::string_view text) const;

    ServiceHostOptions opts_;
    std::unique_ptr<ProcessEngine> engine_;
    std::mutex write_mu_;
    void* client_ = nullptr;
    std::atomic<bool> core_running_ = false;
    SessionId session_ = 0;
    std::jthread thread_;
};

} // namespace ft::win
