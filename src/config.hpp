#pragma once

#include "environment.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tatr {

struct Task {
    std::string description;
    std::vector<std::string> depends;
    // Empty means a grouping task; an explicitly empty exec array is invalid.
    std::vector<std::string> exec;
    // Resolved against the config file's directory; absent inherits caller cwd.
    std::optional<std::filesystem::path> cwd;
    std::optional<EnvironmentOverrides> env;
};

struct Config {
    // Bytewise UTF-8 key order makes listing deterministic on every platform.
    std::map<std::string, Task> tasks;
};

Config load_config(const std::filesystem::path &file);

} // namespace tatr
