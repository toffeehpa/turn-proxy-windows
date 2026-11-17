#include "core/log_parser.h"

#include <algorithm>
#include <charconv>

namespace ft {

namespace {

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool has_go_timestamp(std::string_view s) {
    if (s.size() < 20) {
        return false;
    }
    static constexpr std::string_view kPattern = "dddd/dd/dd dd:dd:dd ";
    for (std::size_t i = 0; i < kPattern.size(); ++i) {
        if (kPattern[i] == 'd' ? !is_digit(s[i]) : s[i] != kPattern[i]) {
            return false;
        }
    }
    return true;
}

bool number_after(std::string_view s, std::string_view prefix, int& out) {
    const auto pos = s.find(prefix);
    if (pos == std::string_view::npos) {
        return false;
    }
    const char* first = s.data() + pos + prefix.size();
    const char* last = s.data() + s.size();
    auto [ptr, ec] = std::from_chars(first, last, out);
    return ec == std::errc() && ptr != first;
}

bool contains(std::string_view s, std::string_view what) {
    return s.find(what) != std::string_view::npos;
}

constexpr std::size_t kMaxErrorLength = 300;

std::string clip(std::string_view s) {
    if (s.size() <= kMaxErrorLength) {
        return std::string(s);
    }
    std::size_t cut = kMaxErrorLength;
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) {
        --cut;
    }
    return std::string(s.substr(0, cut)) + "...";
}

} // namespace

CoreLogLine split_core_log_line(std::string_view line) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.remove_suffix(1);
    }
    if (has_go_timestamp(line)) {
        line.remove_prefix(20);
    }
    struct Prefix {
        std::string_view tag;
        LogLevel level;
    };
    static constexpr Prefix kPrefixes[] = {
        {"[DEBUG] ", LogLevel::Debug},
        {"[INFO] ", LogLevel::Info},
        {"[WARN] ", LogLevel::Warn},
        {"[ERROR] ", LogLevel::Error},
    };
    for (const auto& p : kPrefixes) {
        if (line.starts_with(p.tag)) {
            return {p.level, line.substr(p.tag.size())};
        }
    }
    return {LogLevel::None, line};
}

void CoreLogParser::reset() {
    status_ = {};
    udp_streams_.clear();
}

void CoreLogParser::set_active(int n) {
    status_.active = std::max(n, 0);
    if (status_.active > 0) {
        // Поток поднялся - капча, если была, уже позади.
        status_.captcha = false;
    }
}

bool CoreLogParser::feed(std::string_view raw) {
    const EngineStatus before = status_;
    const auto [level, text] = split_core_log_line(raw);

    int n = 0;
    if (number_after(text, "lifecycle start mode=", n) || contains(text, "lifecycle start ")) {
        if (number_after(text, "streams=", n)) {
            status_.total = n;
        }
    } else if (contains(text, "lifecycle stop ")) {
        udp_streams_.clear();
        set_active(0);
    } else if (contains(text, "Established DTLS connection")) {
        if (number_after(text, "[STREAM ", n)) {
            udp_streams_.insert(n);
            set_active(static_cast<int>(udp_streams_.size()));
        }
    } else if (contains(text, "Closed DTLS connection")) {
        if (number_after(text, "[STREAM ", n)) {
            udp_streams_.erase(n);
            set_active(static_cast<int>(udp_streams_.size()));
        }
    } else if (contains(text, "(active: ") &&
               (contains(text, "] connected (") || contains(text, "] disconnected (") ||
                contains(text, "pool serving traffic"))) {
        if (number_after(text, "(active: ", n)) {
            set_active(n);
        }
    } else if (contains(text, "[Captcha] Triggering manual captcha fallback")) {
        status_.captcha = true;
    } else if (contains(text, "[Captcha] Got token from browser")) {
        status_.captcha = false;
    }

    if (contains(text, "terminal link error")) {
        status_.link_error = true;
    }
    if (contains(text, "[Captcha] FATAL") || text.starts_with("fatal: ")) {
        status_.fatal = true;
        status_.last_error = clip(text);
    } else if (level == LogLevel::Error) {
        status_.last_error = clip(text);
    } else if (level == LogLevel::None && !text.empty() && has_go_timestamp(raw)) {
        // log.Fatalf без уровня - ошибки конфига до старта сессии.
        status_.last_error = clip(text);
    }
    return status_ != before;
}

void SecretRedactor::add(std::string_view secret) {
    if (secret.size() < 6) {
        return;
    }
    if (std::ranges::find(secrets_, secret) == secrets_.end()) {
        secrets_.emplace_back(secret);
        // Длинные первыми: ссылка может содержать другой секрет как подстроку.
        std::ranges::sort(secrets_, [](const auto& a, const auto& b) { return a.size() > b.size(); });
    }
}

std::string SecretRedactor::apply(std::string_view line) const {
    std::string out(line);
    for (const auto& s : secrets_) {
        std::size_t pos = 0;
        while ((pos = out.find(s, pos)) != std::string::npos) {
            out.replace(pos, s.size(), "***");
            pos += 3;
        }
    }
    return out;
}

} // namespace ft
