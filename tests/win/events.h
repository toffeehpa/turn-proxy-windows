#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

#include "core/engine.h"

namespace ft::test {

// События движка из рабочих потоков с ожиданием нужного - общий помощник Windows-тестов.
struct Events {
    std::mutex mu;
    std::condition_variable cv;
    std::vector<EngineEvent> list;

    void push(EngineEvent e) {
        std::lock_guard lock(mu);
        list.push_back(std::move(e));
        cv.notify_all();
    }
    template <class Pred>
    bool wait(Pred pred, int seconds = 15) {
        std::unique_lock lock(mu);
        return cv.wait_for(lock, std::chrono::seconds(seconds), [&] { return pred(list); });
    }
    bool has_line(std::string_view text) {
        return wait([&](const auto& l) {
            for (const auto& e : l) {
                if (e.kind == EngineEvent::Kind::Output && e.line.find(text) != std::string::npos) {
                    return true;
                }
            }
            return false;
        });
    }
    std::optional<int> exit_code() {
        if (!wait([](const auto& l) { return !l.empty() && l.back().kind == EngineEvent::Kind::Exited; })) {
            return std::nullopt;
        }
        std::lock_guard lock(mu);
        return list.back().exit_code;
    }
};

} // namespace ft::test
