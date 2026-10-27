#pragma once

#include "config.hpp"

#include <ostream>
#include <span>
#include <string_view>

namespace tatr {

int run_task(const Config &config, std::string_view target, std::span<const std::string> arguments,
             std::ostream &console);

} // namespace tatr
