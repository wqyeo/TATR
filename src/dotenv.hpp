#pragma once

#include "environment.hpp"

#include <filesystem>

namespace tatr {

// Strict, single-line dotenv dialect. Returns owned values, with override keys
// normalized through environment_name_key for native case-aware layer merging.
// Errors include the file and line where available, never assignment contents.
EnvironmentOverrides load_dotenv(const std::filesystem::path &file);

} // namespace tatr
