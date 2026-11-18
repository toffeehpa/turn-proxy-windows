#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include "core/profile.h"

namespace ft {

using SessionId = std::uint64_t;

// События движка. Адаптер шлёт их из своих потоков, хост переносит на UI-поток
// (PostMessage) - ConnectionManager их видит только там.
struct EngineEvent {
    enum class Kind { Output, Exited };

    Kind kind = Kind::Output;
    SessionId session = 0;
    std::string line;
    int exit_code = 0;
};

struct EngineLaunch {
    // Служба строит аргументы сама из профиля: от клиента без прав готовую командную строку не берём.
    Profile profile;
    std::vector<std::string> args;
    // Для лога: секретные флаги уже замаскированы.
    std::vector<std::string> display_args;
};

class IEngine {
public:
    virtual ~IEngine() = default;

    // Синхронно запускает процесс. Ошибка - текст для пользователя и лога, без секретов.
    virtual std::expected<void, std::string> start(SessionId session, const EngineLaunch& launch) = 0;

    // Асинхронно: штатный сигнал, тайм-аут, потом жёсткое завершение. Итог - событие Exited.
    virtual void stop() = 0;
};

// Один таймер на переподключение; хост вызывает ConnectionManager::on_reconnect_timer.
class IReconnectTimer {
public:
    virtual ~IReconnectTimer() = default;
    virtual void schedule(std::uint32_t delay_ms) = 0;
    virtual void cancel() = 0;
};

} // namespace ft
