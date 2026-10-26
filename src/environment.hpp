#pragma once

#include <map>
#include <string>
#include <string_view>

namespace tatr {

using EnvironmentOverrides = std::map<std::string, std::string>;

constexpr bool valid_environment_name(std::string_view name) noexcept {
    const auto letter = [](char value) {
        return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || value == '_';
    };
    if (name.empty() || !letter(name.front()))
        return false;
    for (const char value : name) {
        if (!letter(value) && !(value >= '0' && value <= '9'))
            return false;
    }
    return true;
}

// Override names are ASCII; inherited Windows names use native Unicode comparison.
inline std::string environment_name_key(std::string_view name) {
    std::string key{name};
#ifdef _WIN32
    for (auto &value : key) {
        if (value >= 'A' && value <= 'Z')
            value = static_cast<char>(value + ('a' - 'A'));
    }
#endif
    return key;
}

} // namespace tatr
