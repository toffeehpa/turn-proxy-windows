#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <thread>

#include "core/engine.h"

namespace ft::win {

// Движок - freeturn-core.exe в отдельном процессе внутри Job Object. Про окна не знает:
// события уходят в sink из рабочих потоков.
class ProcessEngine : public IEngine {
public:
    using Sink = std::function<void(EngineEvent)>;

    // Ядро ловит Ctrl+Break и само закрывает сессию за 5 с; запас сверху - на выход процесса.
    static constexpr unsigned kStopTimeoutMs = 7000;

    ProcessEngine(std::function<std::filesystem::path()> core_path, std::filesystem::path work_dir,
                  Sink sink);
    ~ProcessEngine() override;
    ProcessEngine(const ProcessEngine&) = delete;
    ProcessEngine& operator=(const ProcessEngine&) = delete;

    std::expected<void, std::string> start(SessionId session, const EngineLaunch& launch) override;
    void stop() override;

    // Синхронная остановка для выхода из приложения.
    void shutdown(unsigned timeout_ms);

private:
    std::function<std::filesystem::path()> core_path_;
    std::filesystem::path work_dir_;
    Sink sink_;
    std::atomic<bool> running_ = false;
    std::atomic<unsigned> stop_timeout_ms_ = kStopTimeoutMs;
    std::jthread monitor_;
};

// Ctrl+Break группе процесса pid: Go-ядро видит его как SIGINT.
bool send_ctrl_break(unsigned long pid);

} // namespace ft::win
