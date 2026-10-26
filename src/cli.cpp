#include "cli.hpp"
#include "config.hpp"
#include "planner.hpp"
#include "runner.hpp"

#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace tatr {
namespace {

constexpr std::string_view help = R"(tatr - That's Another Task Runner

Usage:
  tatr [--file PATH] list
  tatr [--file PATH] plan TASK
  tatr [--file PATH] run TASK
  tatr --help
  tatr --version

Defaults to tatr.toml in the current directory; no parent-directory search.
Run executes argument arrays directly, in dependency order, stopping on failure.
Task cwd paths resolve from the configuration file's directory; absent inherits cwd.
Task env tables override selected inherited variables for that child only.
)";

} // namespace

int run_cli(std::span<const std::string> arguments, std::ostream &out) {
    if (arguments.empty() || (arguments.size() == 1 && arguments[0] == "--help")) {
        out << help;
        return 0;
    }
    if (arguments.size() == 1 && arguments[0] == "--version") {
        out << "tatr " << TATR_VERSION << '\n';
        return 0;
    }
    std::filesystem::path file = "tatr.toml";
    bool file_set = false;
    std::string command;
    std::string target;
    bool target_set = false;
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const std::string_view arg = arguments[i];
        if (arg == "--file") {
            if (file_set) {
                throw std::runtime_error("--file may only be supplied once");
            }
            if (++i == arguments.size() || arguments[i].empty() || arguments[i].starts_with("--")) {
                throw std::runtime_error("--file requires a path");
            }
            file = std::filesystem::path{std::u8string{arguments[i].begin(), arguments[i].end()}};
            file_set = true;
        } else if (command.empty() && (arg == "list" || arg == "plan" || arg == "run")) {
            command = arg;
        } else if (arg.starts_with('-')) {
            throw std::runtime_error("unknown or misplaced option '" + std::string{arg} +
                                     "'; see --help");
        } else if ((command == "plan" || command == "run") && !target_set) {
            target = arg;
            target_set = true;
        } else {
            throw std::runtime_error("unexpected argument '" + std::string{arg} + "'; see --help");
        }
    }
    if (command.empty()) {
        throw std::runtime_error("expected 'list', 'plan', or 'run'; see --help");
    }
    if ((command == "plan" || command == "run") && (!target_set || target.empty())) {
        throw std::runtime_error(command + " requires a task name; see --help");
    }
    // Validate the complete file and plan before printing any task output.
    const auto config = load_config(file);
    if (command == "run") {
        run_task(config, target);
    } else if (command == "plan") {
        const auto order = plan_task(config, target);
        for (const auto &name : order) {
            out << name << '\n';
        }
    } else {
        for (const auto &[name, task] : config.tasks) {
            out << name;
            if (!task.description.empty()) {
                out << " - " << task.description;
            }
            out << '\n';
        }
    }
    if (!out) {
        throw std::runtime_error("cannot write task output");
    }
    return 0;
}

} // namespace tatr
