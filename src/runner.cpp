#include "runner.hpp"
#include "dotenv.hpp"
#include "planner.hpp"
#include "process.hpp"

#include <algorithm>
#include <chrono>
#include <vector>

namespace tatr {
namespace {
using Timer = std::chrono::steady_clock;
unsigned long long elapsed(Timer::time_point start) {
    return static_cast<unsigned long long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(Timer::now() - start).count());
}

struct Result {
    int status = 0;
    ProcessResult child;
    std::string failed_task;
    std::string message;
    std::string_view outcome() const {
        return status == 0 ? "success" : status == 1 ? "child_failure" : "runner_error";
    }
};

std::optional<EnvironmentOverrides> prepare_environment(const Command &command,
                                                        const EnvironmentOverrides &context) {
    std::optional<EnvironmentOverrides> environment;
    if (command.env_file)
        environment = load_dotenv(*command.env_file);
    if (command.env) {
        if (!environment)
            environment.emplace();
        for (const auto &[key, value] : *command.env)
            environment->insert_or_assign(environment_name_key(key), value);
    }
    if (!context.empty()) {
        if (!environment)
            environment.emplace();
        for (const auto &[key, value] : context)
            environment->insert_or_assign(environment_name_key(key), value);
    }
    return environment;
}

Result execute(const Command &command, std::string_view kind, std::string_view name,
               std::span<const std::string> arguments, const EnvironmentOverrides &context) {
    Result result;
    const auto label = std::string{kind} + " '" + std::string{name} + "'";
    try {
        std::vector<std::string> combined;
        const auto *exec = std::get_if<std::vector<std::string>>(&command.action);
        std::span<const std::string> process_arguments =
            exec ? std::span<const std::string>{*exec} : std::span<const std::string>{};
        if (!arguments.empty()) {
            if (!exec)
                throw std::runtime_error("SMTP actions do not accept runtime arguments");
            combined = *exec;
            combined.insert(combined.end(), arguments.begin(), arguments.end());
            process_arguments = combined;
        }
        const auto environment = prepare_environment(command, context);
        if (const auto *smtp = std::get_if<Smtp>(&command.action)) {
            std::string body = "TATR deployment result\n";
            for (const auto &[key, heading] :
                 {std::pair{"TATR_TARGET", "Target"}, std::pair{"TATR_OUTCOME", "Outcome"},
                  std::pair{"TATR_STATUS", "Status"}, std::pair{"TATR_FAILED_TASK", "Failed task"},
                  std::pair{"TATR_DURATION_MS", "Task duration (ms)"}}) {
                const auto found = context.find(key);
                if (found != context.end())
                    body += std::string{heading} + ": " + found->second + "\n";
            }
            result.child = send_smtp(*smtp, command.cwd, environment, body);
        } else {
            result.child = run_process(process_arguments, command.cwd, environment);
        }
        if (result.child.signal == 0 && result.child.exit_code == 0 &&
            std::holds_alternative<Smtp>(command.action))
            result.message = "SMTP client reported server acceptance";
        if (result.child.signal != 0 || result.child.exit_code != 0) {
            result.status = 1;
            result.message =
                label + (std::holds_alternative<Smtp>(command.action) ? ": SMTP client " : ": ") +
                (result.child.signal != 0
                     ? "terminated by signal " + std::to_string(result.child.signal)
                     : "exited with status " + std::to_string(result.child.exit_code));
        }
    } catch (const std::exception &error) {
        result.status = 2;
        result.message = label;
        if (command.cwd) {
            const auto encoded = command.cwd->u8string();
            result.message += " (cwd '" + std::string{encoded.begin(), encoded.end()} + "')";
        }
        result.message += ": " + std::string{error.what()};
    }
    return result;
}

void finished(Logger &logger, LogLevel level, std::string_view event, std::string_view kind,
              std::string_view name, const Result &result, Timer::time_point start) {
    logger.write({level, event, kind, name, result.outcome(), elapsed(start),
                  result.status != 2 && result.child.signal == 0
                      ? std::optional<unsigned long long>{result.child.exit_code}
                      : std::nullopt,
                  result.status != 2 && result.child.signal != 0
                      ? std::optional<int>{result.child.signal}
                      : std::nullopt,
                  result.message});
}

void diagnostic(std::ostream &console, std::string_view message) {
    console << "tatr: " << log_quote(message) << '\n' << std::flush;
    if (!console)
        throw LogError("logging: cannot write fallback diagnostic");
}

Result execute_tasks(const Config &config, std::string_view target,
                     std::span<const std::string> arguments, const std::vector<std::string> &order,
                     Logger &logger) {
    for (const auto &name : order) {
        const auto &command = config.tasks.at(name).command;
        if (!command.executable())
            continue;
        const auto start = Timer::now();
        logger.write({LogLevel::info, "task_started", "task", name});
        auto result = execute(command, "task", name,
                              name == target ? arguments : std::span<const std::string>{}, {});
        finished(logger, result.status == 0 ? LogLevel::info : LogLevel::error, "task_finished",
                 "task", name, result, start);
        if (result.status != 0) {
            result.failed_task = name;
            return result;
        }
    }
    return {};
}

Result execute_hooks(const Config &config, std::string_view target, const Result &original,
                     unsigned long long duration, Logger &logger, std::ostream &console) {
    const EnvironmentOverrides context{
        {"TATR_TARGET", std::string{target}},
        {"TATR_OUTCOME", std::string{original.outcome()}},
        {"TATR_STATUS", std::to_string(original.status)},
        {"TATR_FAILED_TASK", original.failed_task},
        {"TATR_CHILD_EXIT_CODE", original.status == 1 && original.child.signal == 0
                                     ? std::to_string(original.child.exit_code)
                                     : ""},
        {"TATR_CHILD_SIGNAL", original.status == 1 && original.child.signal != 0
                                  ? std::to_string(original.child.signal)
                                  : ""},
        {"TATR_DURATION_MS", std::to_string(duration)}};
    Result required_failure;
    for (const auto &hook : config.hooks) {
        if ((!hook.targets.empty() &&
             std::ranges::find(hook.targets, target) == hook.targets.end()) ||
            (hook.when == HookWhen::success && original.status != 0) ||
            (hook.when == HookWhen::failure && original.status == 0))
            continue;
        const auto start = Timer::now();
        logger.write({LogLevel::info, "hook_started", "hook", hook.name});
        auto result = execute(hook.command, "hook", hook.name, {}, context);
        const auto level = result.status == 0                 ? LogLevel::info
                           : hook.on_error == HookError::warn ? LogLevel::warn
                                                              : LogLevel::error;
        finished(logger, level, "hook_finished", "hook", hook.name, result, start);
        if (result.status != 0) {
            if (!logger.enabled(level))
                diagnostic(console, result.message);
            if (hook.on_error == HookError::fail && result.status > required_failure.status)
                required_failure = std::move(result);
        }
    }
    return required_failure;
}
} // namespace

int run_task(const Config &config, std::string_view target, std::span<const std::string> arguments,
             std::ostream &console) {
    const auto order = plan_task(config, target);
    if (!arguments.empty() && !config.tasks.at(std::string{target}).command.executable())
        throw std::runtime_error("task '" + std::string{target} +
                                 "': cannot pass runtime arguments to a grouping task");
    if (!arguments.empty() &&
        std::holds_alternative<Smtp>(config.tasks.at(std::string{target}).command.action))
        throw std::runtime_error("SMTP tasks do not accept runtime arguments");
    Logger logger(config.logging, console);
    const auto start = Timer::now();
    logger.write({LogLevel::info, "run_started", "target", target});
    const auto original = execute_tasks(config, target, arguments, order, logger);
    const auto hooks = execute_hooks(config, target, original, elapsed(start), logger, console);
    const auto &final = original.status != 0 ? original : hooks;
    logger.write({final.status == 0 ? LogLevel::info : LogLevel::error,
                  "run_finished",
                  "target",
                  target,
                  original.status == 0 && hooks.status != 0 ? "hook_failure" : original.outcome(),
                  elapsed(start),
                  {},
                  {},
                  final.message,
                  config.hooks.empty() ? std::string_view{} : original.outcome()});
    logger.finish();
    if (original.status != 0 && !logger.enabled(LogLevel::error))
        diagnostic(console, original.message);
    return final.status;
}
} // namespace tatr
