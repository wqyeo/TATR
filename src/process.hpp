#pragma once

#include "environment.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace tatr {

struct ProcessResult {
    std::uint32_t exit_code = 0;
    int signal = 0;
};

struct ProcessInput {
    std::string_view bytes;
    bool quiet = false;
};

// Synchronous direct execution, with inherited standard I/O and environment
// except for supplied child-only overrides. Never mutates the parent's environment.
// A supplied absolute cwd applies only to the child; absent inherits caller cwd.
// Optional supplied bytes replace stdin through a scoped pipe; quiet routes both
// child output streams to the OS null device. Ordinary calls retain inherited I/O.
// Throws on launch/wait/input errors; otherwise returns the child's native status.
ProcessResult run_process(std::span<const std::string> arguments,
                          const std::optional<std::filesystem::path> &cwd = std::nullopt,
                          const std::optional<EnvironmentOverrides> &env = std::nullopt,
                          const std::optional<ProcessInput> &input = std::nullopt);

} // namespace tatr
