#pragma once

#include "core/profile.h"

namespace ft::test {

inline const std::string kKey = "d823fa0000000000000000000000000000000000000000000000000000000001";

inline Profile vk_profile(std::string name = "Home") {
    Profile p;
    p.id = "0123456789abcdef0123456789abcdef";
    p.name = std::move(name);
    p.peer = "203.0.113.7:56000";
    p.obf_profile = "rtpopus3";
    p.secrets.obf_key = kKey;
    p.secrets.client_id = "a1b2c3d4e5f60718293a4b5c6d7e8f90";
    p.secrets.links = {"https://example.invalid/call/join/AAAA"};
    return p;
}

} // namespace ft::test
