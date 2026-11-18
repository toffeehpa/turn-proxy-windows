#pragma once

#include <string>

#include "win/handle.h"

namespace ft::win {

// Мьютекс в Local\ - один FreeTurn на сеанс пользователя.
class SingleInstance {
public:
    SingleInstance();
    ~SingleInstance();
    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    bool is_first() const { return first_; }

private:
    UniqueHandle mutex_;
    bool first_ = false;
};

// SID текущего пользователя строкой (S-1-5-21-...), пусто при ошибке.
std::wstring current_user_sid();

} // namespace ft::win
