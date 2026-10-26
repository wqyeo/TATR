#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace tatr {

struct ProcessResult {
    std::uint32_t exit_code = 0;
    int signal = 0;
};

// Synchronous direct execution, with inherited environment, cwd, and standard I/O.
// Throws on launch/wait errors; otherwise returns the child's native status.
ProcessResult run_process(std::span<const std::string> arguments);

} // namespace tatr
