#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace ft {

enum class LogLevel { None, Debug, Info, Warn, Error };

struct CoreLogLine {
    LogLevel level = LogLevel::None;
    std::string_view text;
};

// Строка ядра: "2006/01/02 15:04:05 [INFO] текст" (stdlib log + logx).
CoreLogLine split_core_log_line(std::string_view line);

struct EngineStatus {
    int active = 0;
    int total = 0;
    bool captcha = false;
    bool fatal = false;
    // VK отверг ссылку звонка: недействительна, звонок завершён или переполнен.
    bool link_error = false;
    std::string last_error;

    bool operator==(const EngineStatus&) const = default;
};

// Состояние сессии из логов CLI-ядра: машиночитаемого статуса у freeturn-core.exe нет.
// UDP-потоки видны только в -debug (Established/Closed DTLS), TCP-сессии - в INFO.
class CoreLogParser {
public:
    bool feed(std::string_view line);
    const EngineStatus& status() const { return status_; }
    void reset();

private:
    void set_active(int n);

    EngineStatus status_;
    std::set<int> udp_streams_;
};

// Вырезает известные секреты профиля из строк перед записью в лог.
class SecretRedactor {
public:
    void add(std::string_view secret);
    std::string apply(std::string_view line) const;

private:
    std::vector<std::string> secrets_;
};

} // namespace ft
