#include "core/json.h"

#include <charconv>
#include <cmath>
#include <limits>

namespace ft::json {

namespace {

const std::string kEmptyString;
const Array kEmptyArray;
const Object kEmptyObject;

class Parser {
public:
    explicit Parser(std::string_view text) : s_(text) {}

    std::expected<Value, ParseError> run() {
        skip_ws();
        auto v = value(0);
        if (!v) {
            return v;
        }
        skip_ws();
        if (pos_ != s_.size()) {
            return fail("trailing characters");
        }
        return v;
    }

private:
    std::unexpected<ParseError> fail(std::string msg) const {
        return std::unexpected(ParseError{pos_, std::move(msg)});
    }

    void skip_ws() {
        while (pos_ < s_.size() &&
               (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) {
            ++pos_;
        }
    }

    bool consume(std::string_view lit) {
        if (s_.substr(pos_, lit.size()) == lit) {
            pos_ += lit.size();
            return true;
        }
        return false;
    }

    std::expected<Value, ParseError> value(std::size_t depth) {
        if (depth > kMaxDepth) {
            return fail("nesting too deep");
        }
        if (pos_ >= s_.size()) {
            return fail("unexpected end");
        }
        switch (s_[pos_]) {
        case '{':
            return object(depth);
        case '[':
            return array(depth);
        case '"': {
            auto str = string();
            if (!str) {
                return std::unexpected(str.error());
            }
            return Value(std::move(*str));
        }
        case 't':
            if (consume("true")) {
                return Value(true);
            }
            return fail("bad literal");
        case 'f':
            if (consume("false")) {
                return Value(false);
            }
            return fail("bad literal");
        case 'n':
            if (consume("null")) {
                return Value(nullptr);
            }
            return fail("bad literal");
        default:
            return number();
        }
    }

    std::expected<Value, ParseError> object(std::size_t depth) {
        ++pos_;
        Object obj;
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] == '}') {
            ++pos_;
            return Value(std::move(obj));
        }
        for (;;) {
            skip_ws();
            if (pos_ >= s_.size() || s_[pos_] != '"') {
                return fail("expected key");
            }
            auto key = string();
            if (!key) {
                return std::unexpected(key.error());
            }
            skip_ws();
            if (pos_ >= s_.size() || s_[pos_] != ':') {
                return fail("expected ':'");
            }
            ++pos_;
            skip_ws();
            auto v = value(depth + 1);
            if (!v) {
                return v;
            }
            // Дубликат ключа - последний выигрывает, как в Go encoding/json.
            bool replaced = false;
            for (auto& [k, existing] : obj) {
                if (k == *key) {
                    existing = std::move(*v);
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                obj.emplace_back(std::move(*key), std::move(*v));
            }
            skip_ws();
            if (pos_ < s_.size() && s_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < s_.size() && s_[pos_] == '}') {
                ++pos_;
                return Value(std::move(obj));
            }
            return fail("expected ',' or '}'");
        }
    }

    std::expected<Value, ParseError> array(std::size_t depth) {
        ++pos_;
        Array arr;
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] == ']') {
            ++pos_;
            return Value(std::move(arr));
        }
        for (;;) {
            skip_ws();
            auto v = value(depth + 1);
            if (!v) {
                return v;
            }
            arr.push_back(std::move(*v));
            skip_ws();
            if (pos_ < s_.size() && s_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < s_.size() && s_[pos_] == ']') {
                ++pos_;
                return Value(std::move(arr));
            }
            return fail("expected ',' or ']'");
        }
    }

    static void append_utf8(std::string& out, std::uint32_t cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool hex4(std::uint32_t& out) {
        if (pos_ + 4 > s_.size()) {
            return false;
        }
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = s_[pos_++];
            v <<= 4;
            if (c >= '0' && c <= '9') {
                v |= static_cast<std::uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                v |= static_cast<std::uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                v |= static_cast<std::uint32_t>(c - 'A' + 10);
            } else {
                return false;
            }
        }
        out = v;
        return true;
    }

    std::expected<std::string, ParseError> string() {
        ++pos_;
        std::string out;
        while (pos_ < s_.size()) {
            const char c = s_[pos_++];
            if (c == '"') {
                return out;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                return std::unexpected(ParseError{pos_ - 1, "control character in string"});
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (pos_ >= s_.size()) {
                break;
            }
            switch (s_[pos_++]) {
            case '"':
                out.push_back('"');
                break;
            case '\\':
                out.push_back('\\');
                break;
            case '/':
                out.push_back('/');
                break;
            case 'b':
                out.push_back('\b');
                break;
            case 'f':
                out.push_back('\f');
                break;
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'u': {
                std::uint32_t cp = 0;
                if (!hex4(cp)) {
                    return std::unexpected(ParseError{pos_, "bad \\u escape"});
                }
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    std::uint32_t lo = 0;
                    if (!consume("\\u") || !hex4(lo) || lo < 0xDC00 || lo > 0xDFFF) {
                        return std::unexpected(ParseError{pos_, "bad surrogate pair"});
                    }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    return std::unexpected(ParseError{pos_, "lone low surrogate"});
                }
                append_utf8(out, cp);
                break;
            }
            default:
                return std::unexpected(ParseError{pos_ - 1, "bad escape"});
            }
        }
        return std::unexpected(ParseError{pos_, "unterminated string"});
    }

    std::expected<Value, ParseError> number() {
        const std::size_t start = pos_;
        if (pos_ < s_.size() && s_[pos_] == '-') {
            ++pos_;
        }
        if (pos_ >= s_.size() || s_[pos_] < '0' || s_[pos_] > '9') {
            return fail("unexpected character");
        }
        if (s_[pos_] == '0') {
            ++pos_;
        } else {
            while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') {
                ++pos_;
            }
        }
        if (pos_ < s_.size() && s_[pos_] == '.') {
            ++pos_;
            if (pos_ >= s_.size() || s_[pos_] < '0' || s_[pos_] > '9') {
                return fail("bad fraction");
            }
            while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') {
                ++pos_;
            }
        }
        if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) {
                ++pos_;
            }
            if (pos_ >= s_.size() || s_[pos_] < '0' || s_[pos_] > '9') {
                return fail("bad exponent");
            }
            while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') {
                ++pos_;
            }
        }
        double d = 0;
        const char* first = s_.data() + start;
        const char* last = s_.data() + pos_;
        auto [ptr, ec] = std::from_chars(first, last, d);
        if (ec != std::errc() || ptr != last || !std::isfinite(d)) {
            return std::unexpected(ParseError{start, "bad number"});
        }
        return Value(d);
    }

    std::string_view s_;
    std::size_t pos_ = 0;
};

void escape(std::string& out, std::string_view s) {
    out.push_back('"');
    for (const char c : s) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                static constexpr char kHex[] = "0123456789abcdef";
                out += "\\u00";
                out.push_back(kHex[(static_cast<unsigned char>(c) >> 4) & 0xF]);
                out.push_back(kHex[static_cast<unsigned char>(c) & 0xF]);
            } else {
                out.push_back(c);
            }
        }
    }
    out.push_back('"');
}

void write_number(std::string& out, double d) {
    // Целые пишем без экспоненты и дробной части - так их ждут Go и человек.
    if (std::trunc(d) == d && std::fabs(d) < 9.0e15) {
        out += std::to_string(static_cast<long long>(d));
        return;
    }
    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), d);
    if (ec != std::errc()) {
        out += "0";
        return;
    }
    out.append(buf, ptr);
}

void newline(std::string& out, int indent, int level) {
    if (indent < 0) {
        return;
    }
    out.push_back('\n');
    out.append(static_cast<std::size_t>(indent) * static_cast<std::size_t>(level), ' ');
}

void write(std::string& out, const Value& v, int indent, int level) {
    switch (v.kind()) {
    case Value::Kind::Null:
        out += "null";
        break;
    case Value::Kind::Bool:
        out += v.as_bool() ? "true" : "false";
        break;
    case Value::Kind::Number:
        write_number(out, v.as_double());
        break;
    case Value::Kind::String:
        escape(out, v.as_string());
        break;
    case Value::Kind::List: {
        const auto& a = v.as_array();
        if (a.empty()) {
            out += "[]";
            break;
        }
        out.push_back('[');
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (i > 0) {
                out.push_back(',');
            }
            newline(out, indent, level + 1);
            write(out, a[i], indent, level + 1);
        }
        newline(out, indent, level);
        out.push_back(']');
        break;
    }
    case Value::Kind::Map: {
        const auto& o = v.as_object();
        if (o.empty()) {
            out += "{}";
            break;
        }
        out.push_back('{');
        for (std::size_t i = 0; i < o.size(); ++i) {
            if (i > 0) {
                out.push_back(',');
            }
            newline(out, indent, level + 1);
            escape(out, o[i].first);
            out += indent < 0 ? ":" : ": ";
            write(out, o[i].second, indent, level + 1);
        }
        newline(out, indent, level);
        out.push_back('}');
        break;
    }
    }
}

} // namespace

Value::Kind Value::kind() const {
    return static_cast<Kind>(data_.index());
}

bool Value::as_bool(bool def) const {
    if (const auto* b = std::get_if<bool>(&data_)) {
        return *b;
    }
    return def;
}

double Value::as_double(double def) const {
    if (const auto* d = std::get_if<double>(&data_)) {
        return *d;
    }
    return def;
}

int Value::as_int(int def) const {
    const auto* d = std::get_if<double>(&data_);
    if (d == nullptr || std::trunc(*d) != *d || *d < static_cast<double>(std::numeric_limits<int>::min()) ||
        *d > static_cast<double>(std::numeric_limits<int>::max())) {
        return def;
    }
    return static_cast<int>(*d);
}

const std::string& Value::as_string() const {
    if (const auto* s = std::get_if<std::string>(&data_)) {
        return *s;
    }
    return kEmptyString;
}

std::string Value::as_string_or(std::string_view def) const {
    if (const auto* s = std::get_if<std::string>(&data_)) {
        return *s;
    }
    return std::string(def);
}

const Array& Value::as_array() const {
    if (const auto* a = std::get_if<Array>(&data_)) {
        return *a;
    }
    return kEmptyArray;
}

const Object& Value::as_object() const {
    if (const auto* o = std::get_if<Object>(&data_)) {
        return *o;
    }
    return kEmptyObject;
}

Array& Value::as_array() {
    if (!std::holds_alternative<Array>(data_)) {
        data_ = Array{};
    }
    return std::get<Array>(data_);
}

Object& Value::as_object() {
    if (!std::holds_alternative<Object>(data_)) {
        data_ = Object{};
    }
    return std::get<Object>(data_);
}

const Value* Value::find(std::string_view key) const {
    const auto* o = std::get_if<Object>(&data_);
    if (o == nullptr) {
        return nullptr;
    }
    for (const auto& [k, v] : *o) {
        if (k == key) {
            return &v;
        }
    }
    return nullptr;
}

void Value::set(std::string_view key, Value v) {
    auto& o = as_object();
    for (auto& [k, existing] : o) {
        if (k == key) {
            existing = std::move(v);
            return;
        }
    }
    o.emplace_back(std::string(key), std::move(v));
}

std::expected<Value, ParseError> parse(std::string_view text) {
    // BOM от Блокнота не должен ломать импорт.
    if (text.starts_with("\xEF\xBB\xBF")) {
        text.remove_prefix(3);
    }
    return Parser(text).run();
}

std::string serialize(const Value& v, int indent) {
    std::string out;
    write(out, v, indent, 0);
    return out;
}

} // namespace ft::json
