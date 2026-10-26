#include "runner.hpp"
#include "planner.hpp"
#include "process.hpp"

namespace tatr {

void run_task(const Config &config, std::string_view target) {
    const auto order = plan_task(config, target);
    for (const auto &name : order) {
        const auto &command = config.tasks.at(name).exec;
        if (command.empty())
            continue;
        ProcessResult result;
        try {
            result = run_process(command);
        } catch (const std::exception &error) {
            throw std::runtime_error("task '" + name + "': " + error.what());
        }
        if (result.signal != 0) {
            throw TaskFailure("task '" + name + "': terminated by signal " +
                              std::to_string(result.signal));
        }
        if (result.exit_code != 0) {
            throw TaskFailure("task '" + name + "': exited with status " +
                              std::to_string(result.exit_code));
        }
    }
}

} // namespace tatr
