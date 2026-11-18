#pragma once

// Минимальный раннер: тянуть фреймворк ради пары макросов - лишняя зависимость.

#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace ft::test {

struct Case {
    std::string_view name;
    std::function<void()> fn;
};

std::vector<Case>& registry();
void fail(std::string_view file, int line, std::string_view expr, const std::string& detail = {});

struct Registrar {
    Registrar(std::string_view name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

struct Abort {};

template <class T>
std::string show(const T& v) {
    // Отключённая специализация formatter не конструируется - так проверка работает и в старом MSVC.
    if constexpr (std::is_default_constructible_v<std::formatter<T, char>>) {
        return std::format("{}", v);
    } else if constexpr (std::is_enum_v<T>) {
        return std::format("enum({})", static_cast<long long>(v));
    } else {
        return "?";
    }
}

} // namespace ft::test

#define FT_CAT2(a, b) a##b
#define FT_CAT(a, b) FT_CAT2(a, b)

#define TEST_CASE(name)                                                                                      \
    static void FT_CAT(ft_test_, __LINE__)();                                                                \
    static const ::ft::test::Registrar FT_CAT(ft_reg_, __LINE__)(name, &FT_CAT(ft_test_, __LINE__));         \
    static void FT_CAT(ft_test_, __LINE__)()

#define CHECK(expr)                                                                                          \
    do {                                                                                                     \
        if (!(expr)) {                                                                                       \
            ::ft::test::fail(__FILE__, __LINE__, #expr);                                                     \
        }                                                                                                    \
    } while (false)

#define CHECK_EQ(a, b)                                                                                       \
    do {                                                                                                     \
        const auto& ft_a_ = (a);                                                                             \
        const auto& ft_b_ = (b);                                                                             \
        if (!(ft_a_ == ft_b_)) {                                                                             \
            ::ft::test::fail(__FILE__, __LINE__, #a " == " #b,                                               \
                             ::ft::test::show(ft_a_) + " != " + ::ft::test::show(ft_b_));                    \
        }                                                                                                    \
    } while (false)

#define REQUIRE(expr)                                                                                        \
    do {                                                                                                     \
        if (!(expr)) {                                                                                       \
            ::ft::test::fail(__FILE__, __LINE__, #expr);                                                     \
            throw ::ft::test::Abort{};                                                                       \
        }                                                                                                    \
    } while (false)
