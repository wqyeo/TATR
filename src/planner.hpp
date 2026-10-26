#pragma once

#include "config.hpp"

#include <string_view>

namespace tatr {

void validate_dependencies(const Config &config);
std::vector<std::string> plan_task(const Config &config, std::string_view target);

} // namespace tatr
