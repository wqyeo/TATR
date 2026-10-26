#pragma once

#include "config.hpp"

#include <stdexcept>
#include <string_view>

namespace tatr {

class TaskFailure : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

void run_task(const Config &config, std::string_view target);

} // namespace tatr
