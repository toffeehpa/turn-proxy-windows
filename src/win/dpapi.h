#pragma once

#include "core/profile_store.h"

namespace ft::win {

// DPAPI в контексте текущего пользователя: расшифровать может только он на этом ПК.
class DpapiProtector : public ISecretProtector {
public:
    std::optional<std::string> protect(std::string_view plain) override;
    std::optional<std::string> unprotect(std::string_view blob) override;
};

} // namespace ft::win
