#pragma once

#include "config.hpp"

#include <span>
#include <stdexcept>
#include <string_view>

namespace tatr {

class TaskFailure : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

int run_task(const Config &config, std::string_view target, std::span<const std::string> arguments,
             std::ostream &console);

} // namespace tatr
