#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/profile.h"

namespace ft {

struct CoreArgsOptions {
    // Без -debug ядро не пишет, сколько UDP-потоков поднято, - статус не из чего считать.
    bool debug = true;
    // Маршруты-исключения до TURN-серверов: нужны права администратора, ставит только служба.
    bool routes = false;
};

// Аргументы freeturn-core.exe по профилю, как internal/config/args.go: только отличия от дефолтов.
std::vector<std::string> build_core_args(const Profile& p, const CoreArgsOptions& opts = {});

// Та же строка для лога: значения секретных флагов заменены на ***.
std::vector<std::string> redact_core_args(const std::vector<std::string>& args);

// Квотирование по правилам CommandLineToArgvW - их же понимает Go на Windows.
std::string quote_windows_arg(std::string_view arg);
std::string join_windows_command_line(const std::vector<std::string>& args);

std::vector<std::string> split_windows_command_line(std::string_view cmdline);

} // namespace ft
