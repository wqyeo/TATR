#pragma once

#include "environment.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace tatr {

struct ProcessResult {
    std::uint32_t exit_code = 0;
    int signal = 0;
};

// Synchronous direct execution, with inherited standard I/O and environment
// except for supplied child-only overrides. Never mutates the parent's environment.
// A supplied absolute cwd applies only to the child; absent inherits caller cwd.
// Throws on launch/wait errors; otherwise returns the child's native status.
ProcessResult run_process(std::span<const std::string> arguments,
                          const std::optional<std::filesystem::path> &cwd = std::nullopt,
                          const std::optional<EnvironmentOverrides> &env = std::nullopt);

} // namespace tatr
