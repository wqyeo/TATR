#pragma once

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

// Synchronous direct execution, with inherited environment and standard I/O.
// A supplied absolute cwd applies only to the child; absent inherits caller cwd.
// Throws on launch/wait errors; otherwise returns the child's native status.
ProcessResult run_process(std::span<const std::string> arguments,
                          const std::optional<std::filesystem::path> &cwd = std::nullopt);

} // namespace tatr
