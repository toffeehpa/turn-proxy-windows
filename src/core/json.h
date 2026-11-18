#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace ft::json {

class Value;

using Array = std::vector<Value>;
// Порядок ключей сохраняем: файлы профилей читаются глазами, diff должен быть стабильным.
using Object = std::vector<std::pair<std::string, Value>>;

class Value {
public:
    enum class Kind { Null, Bool, Number, String, List, Map };

    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : data_(b) {}
    Value(int v) : data_(static_cast<double>(v)) {}
    Value(std::int64_t v) : data_(static_cast<double>(v)) {}
    Value(double v) : data_(v) {}
    Value(const char* s) : data_(std::string(s)) {}
    Value(std::string s) : data_(std::move(s)) {}
    Value(std::string_view s) : data_(std::string(s)) {}
    Value(Array a) : data_(std::move(a)) {}
    Value(Object o) : data_(std::move(o)) {}

    Kind kind() const;
    bool is_null() const { return kind() == Kind::Null; }
    bool is_bool() const { return kind() == Kind::Bool; }
    bool is_number() const { return kind() == Kind::Number; }
    bool is_string() const { return kind() == Kind::String; }
    bool is_array() const { return kind() == Kind::List; }
    bool is_object() const { return kind() == Kind::Map; }

    bool as_bool(bool def = false) const;
    double as_double(double def = 0) const;
    // Только целые числа в диапазоне int, иначе def.
    int as_int(int def = 0) const;
    const std::string& as_string() const;
    std::string as_string_or(std::string_view def) const;
    const Array& as_array() const;
    const Object& as_object() const;
    Array& as_array();
    Object& as_object();

    const Value* find(std::string_view key) const;
    void set(std::string_view key, Value v);

private:
    // vector допускает неполный тип элемента - значение копируется глубоко, без shared_ptr.
    std::variant<std::monostate, bool, double, std::string, Array, Object> data_;
};

struct ParseError {
    std::size_t offset = 0;
    std::string message;
};

inline constexpr std::size_t kMaxDepth = 64;

std::expected<Value, ParseError> parse(std::string_view text);

std::string serialize(const Value& v, int indent = 2);

} // namespace ft::json
