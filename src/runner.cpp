#include "runner.hpp"
#include "planner.hpp"
#include "process.hpp"

namespace tatr {

void run_task(const Config &config, std::string_view target) {
    const auto order = plan_task(config, target);
    for (const auto &name : order) {
        const auto &task = config.tasks.at(name);
        const auto &command = task.exec;
        if (command.empty())
            continue;
        ProcessResult result;
        try {
            result = run_process(command, task.cwd, task.env);
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
