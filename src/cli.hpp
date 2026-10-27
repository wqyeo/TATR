#pragma once

#include <ostream>
#include <span>
#include <string>

namespace tatr {

int run_cli(std::span<const std::string> arguments, std::ostream &out, std::ostream &err);

} // namespace tatr
