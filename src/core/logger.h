#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

#include "core/log_parser.h"

namespace ft {

// Файловый лог с ротацией: freeturn.log и один freeturn.log.1. Потокобезопасный.
class Logger {
public:
    static constexpr std::uintmax_t kMaxSize = 1024 * 1024;

    bool open(const std::filesystem::path& file);
    void write(LogLevel level, std::string_view text);
    void set_debug(bool on) { debug_ = on; }
    const std::filesystem::path& path() const { return path_; }

private:
    void rotate_if_needed();

    std::mutex mu_;
    std::filesystem::path path_;
    std::ofstream out_;
    std::uintmax_t size_ = 0;
    bool debug_ = false;
};

std::string_view level_tag(LogLevel level);

} // namespace ft
