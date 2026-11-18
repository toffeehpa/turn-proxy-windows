#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ft {

// Запись через временный файл и замену: оборванная запись не портит старый файл.
// На Windows - src/win/file_io_win.cpp, на остальных ОС только для локальных тестов.
std::expected<void, std::string> write_file_atomic(const std::filesystem::path& file, std::string_view data);

std::optional<std::string> read_file(const std::filesystem::path& file,
                                     std::uintmax_t max_size = 4 * 1024 * 1024);

} // namespace ft
