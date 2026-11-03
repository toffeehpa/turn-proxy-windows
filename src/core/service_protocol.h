#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "core/profile.h"

namespace ft {

// Обмен FreeTurn.exe со службой: одно JSON-сообщение на сообщение пайпа.
inline constexpr std::size_t kMaxServiceMessage = 256 * 1024;

struct ServiceRequest {
    enum class Kind { Start, Stop };

    Kind kind = Kind::Stop;
    Profile profile;
};

struct ServiceEvent {
    enum class Kind { Output, Exited, Error };

    Kind kind = Kind::Output;
    std::string text;
    int code = 0;

    bool operator==(const ServiceEvent&) const = default;
};

std::string encode_request(const ServiceRequest& r);
// Профиль из запроса проходит ту же проверку, что и в GUI: служба не верит клиенту на слово.
std::expected<ServiceRequest, std::string> decode_request(std::string_view msg);

std::string encode_event(const ServiceEvent& e);
std::optional<ServiceEvent> decode_event(std::string_view msg);

} // namespace ft
