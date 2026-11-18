#include "core/logger.h"

#include <chrono>
#include <format>
#include <system_error>

namespace ft {

namespace {

std::string timestamp() {
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    try {
        return std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::current_zone()->to_local(now));
    } catch (...) {
        // Нет базы часовых поясов - пишем UTC, лог важнее красоты.
        return std::format("{:%Y-%m-%d %H:%M:%S}Z", now);
    }
}

} // namespace

std::string_view level_tag(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
    case LogLevel::None:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }
    return "INFO";
}

bool Logger::open(const std::filesystem::path& file) {
    std::lock_guard lock(mu_);
    path_ = file;
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    size_ = std::filesystem::exists(file, ec) ? std::filesystem::file_size(file, ec) : 0;
    if (ec) {
        size_ = 0;
    }
    out_.open(file, std::ios::binary | std::ios::app);
    return out_.is_open();
}

void Logger::rotate_if_needed() {
    if (size_ < kMaxSize) {
        return;
    }
    out_.close();
    std::error_code ec;
    auto old = path_;
    old += ".1";
    std::filesystem::remove(old, ec);
    std::filesystem::rename(path_, old, ec);
    out_.open(path_, std::ios::binary | std::ios::trunc);
    size_ = 0;
}

void Logger::write(LogLevel level, std::string_view text) {
    if (level == LogLevel::Debug && !debug_) {
        return;
    }
    auto line = std::format("{} [{}] {}\r\n", timestamp(), level_tag(level), text);
    std::lock_guard lock(mu_);
    if (!out_.is_open()) {
        return;
    }
    rotate_if_needed();
    out_ << line;
    out_.flush();
    size_ += line.size();
}

} // namespace ft
