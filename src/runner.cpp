#include "runner.hpp"
#include "planner.hpp"
#include "process.hpp"

#include <vector>

namespace tatr {

void run_task(const Config &config, std::string_view target,
              std::span<const std::string> arguments) {
    const auto order = plan_task(config, target);
    if (!arguments.empty() && config.tasks.at(std::string{target}).exec.empty()) {
        throw std::runtime_error("task '" + std::string{target} +
                                 "': cannot pass runtime arguments to a grouping task");
    }
    for (const auto &name : order) {
        const auto &task = config.tasks.at(name);
        const auto &command = task.exec;
        if (command.empty())
            continue;
        ProcessResult result;
        try {
            std::vector<std::string> combined;
            std::span<const std::string> process_arguments = command;
            if (name == target && !arguments.empty()) {
                combined = command;
                combined.insert(combined.end(), arguments.begin(), arguments.end());
                process_arguments = combined;
            }
            result = run_process(process_arguments, task.cwd, task.env);
        } catch (const std::exception &error) {
            std::string context = "task '" + name + "'";
            if (task.cwd) {
                const auto encoded = task.cwd->u8string();
                context += " (cwd '" + std::string{encoded.begin(), encoded.end()} + "')";
            }
            throw std::runtime_error(context + ": " + error.what());
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
