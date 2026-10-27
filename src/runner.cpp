#include "runner.hpp"
#include "dotenv.hpp"
#include "planner.hpp"
#include "process.hpp"

#include <chrono>
#include <vector>

namespace tatr {

namespace {
using Timer = std::chrono::steady_clock;
unsigned long long elapsed(Timer::time_point start) {
    return static_cast<unsigned long long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(Timer::now() - start).count());
}

void execute_tasks(const Config &config, std::string_view target,
                   std::span<const std::string> arguments, const std::vector<std::string> &order,
                   Logger &logger) {
    for (const auto &name : order) {
        const auto &task = config.tasks.at(name);
        const auto &command = task.exec;
        if (command.empty())
            continue;
        const auto start = Timer::now();
        logger.write({LogLevel::info, "task_started", "task", name});
        ProcessResult result;
        std::string failure;
        try {
            std::vector<std::string> combined;
            std::span<const std::string> process_arguments = command;
            if (name == target && !arguments.empty()) {
                combined = command;
                combined.insert(combined.end(), arguments.begin(), arguments.end());
                process_arguments = combined;
            }
            std::optional<EnvironmentOverrides> file_environment;
            if (task.env_file) {
                file_environment = load_dotenv(*task.env_file);
                if (task.env) {
                    for (const auto &[key, value] : *task.env) {
                        file_environment->insert_or_assign(environment_name_key(key), value);
                    }
                }
            }
            result = run_process(process_arguments, task.cwd,
                                 file_environment ? file_environment : task.env);
        } catch (const std::exception &error) {
            failure = "task '" + name + "'";
            if (task.cwd) {
                const auto encoded = task.cwd->u8string();
                failure += " (cwd '" + std::string{encoded.begin(), encoded.end()} + "')";
            }
            failure += ": " + std::string{error.what()};
        }
        if (!failure.empty()) {
            logger.write({LogLevel::error,
                          "task_finished",
                          "task",
                          name,
                          "runner_error",
                          elapsed(start),
                          {},
                          {},
                          failure});
            throw std::runtime_error(failure);
        }
        const bool failed = result.signal != 0 || result.exit_code != 0;
        if (failed) {
            failure =
                "task '" + name + "': " +
                (result.signal != 0 ? "terminated by signal " + std::to_string(result.signal)
                                    : "exited with status " + std::to_string(result.exit_code));
        }
        logger.write({failed ? LogLevel::error : LogLevel::info, "task_finished", "task", name,
                      failed ? "child_failure" : "success", elapsed(start),
                      result.signal == 0 ? std::optional<unsigned long long>{result.exit_code}
                                         : std::nullopt,
                      result.signal != 0 ? std::optional<int>{result.signal} : std::nullopt,
                      failure});
        if (failed)
            throw TaskFailure(failure);
    }
}
} // namespace

int run_task(const Config &config, std::string_view target, std::span<const std::string> arguments,
             std::ostream &console) {
    const auto order = plan_task(config, target);
    if (!arguments.empty() && config.tasks.at(std::string{target}).exec.empty()) {
        throw std::runtime_error("task '" + std::string{target} +
                                 "': cannot pass runtime arguments to a grouping task");
    }
    Logger logger(config.logging, console);
    const auto start = Timer::now();
    logger.write({LogLevel::info, "run_started", "target", target});
    int status = 0;
    std::string failure;
    try {
        execute_tasks(config, target, arguments, order, logger);
    } catch (const TaskFailure &error) {
        status = 1;
        failure = error.what();
    } catch (const LogError &) {
        // Sink failures bypass logging; main reports a direct stderr diagnostic.
        throw;
    } catch (const std::exception &error) {
        status = 2;
        failure = error.what();
    }
    logger.write({status == 0 ? LogLevel::info : LogLevel::error,
                  "run_finished",
                  "target",
                  target,
                  status == 0   ? "success"
                  : status == 1 ? "child_failure"
                                : "runner_error",
                  elapsed(start),
                  {},
                  {},
                  failure});
    logger.finish();
    if (status != 0 && !logger.enabled(LogLevel::error)) {
        console << "tatr: " << log_quote(failure) << '\n' << std::flush;
        if (!console)
            throw LogError("logging: cannot write fallback diagnostic");
    }
    return status;
}

} // namespace tatr
