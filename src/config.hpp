#pragma once

#include "environment.hpp"
#include "logging.hpp"
#include "smtp.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace tatr {

struct Command {
    // Empty means a grouping task; an explicitly empty exec array is invalid.
    std::variant<std::vector<std::string>, Smtp> action;
    bool executable() const {
        const auto *exec = std::get_if<std::vector<std::string>>(&action);
        return !exec || !exec->empty();
    }
    // Resolved against the config file's directory; absent inherits caller cwd.
    std::optional<std::filesystem::path> cwd;
    std::optional<EnvironmentOverrides> env;
    std::optional<std::filesystem::path> env_file;
};

struct Task {
    std::string description;
    std::vector<std::string> depends;
    Command command;
};

enum class HookWhen { success, failure, always };
enum class HookError { warn, fail };

struct Hook {
    std::string name;
    HookWhen when = HookWhen::always;
    std::vector<std::string> targets;
    Command command;
    HookError on_error = HookError::warn;
};

struct Config {
    Logging logging;
    std::vector<Hook> hooks;
    // Bytewise UTF-8 key order makes listing deterministic on every platform.
    std::map<std::string, Task> tasks;
};

Config load_config(const std::filesystem::path &file);

} // namespace tatr
