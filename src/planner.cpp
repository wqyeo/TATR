#include "planner.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace tatr {
namespace {

enum class State { unseen, visiting, complete };

// Explicit frames keep long dependency chains off the native call stack.
class Traversal {
  public:
    explicit Traversal(const Config &config) : config_(config) {}

    void visit(const std::string &root) {
        if (states_[root] == State::complete) {
            return;
        }
        push(root);
        while (!frames_.empty()) {
            auto &frame = frames_.back();
            const auto &dependencies = config_.tasks.at(*frame.name).depends;
            if (frame.next == dependencies.size()) {
                states_[*frame.name] = State::complete;
                order.push_back(*frame.name);
                frames_.pop_back();
                continue;
            }
            const auto &dependency = dependencies[frame.next++];
            if (!config_.tasks.contains(dependency)) {
                throw std::runtime_error("task '" + *frame.name + "': missing dependency '" +
                                         dependency + "'");
            }
            const auto state = states_[dependency];
            if (state == State::visiting) {
                const auto start =
                    std::find_if(frames_.begin(), frames_.end(),
                                 [&](const Frame &entry) { return *entry.name == dependency; });
                std::string message = "dependency cycle: ";
                for (auto it = start; it != frames_.end(); ++it) {
                    message += "'" + *it->name + "' -> ";
                }
                throw std::runtime_error(message + "'" + dependency + "'");
            }
            if (state == State::unseen) {
                push(dependency);
            }
        }
    }

    std::vector<std::string> order;

  private:
    struct Frame {
        const std::string *name;
        std::size_t next;
    };
    void push(const std::string &name) {
        states_[name] = State::visiting;
        frames_.push_back({&name, 0});
    }
    const Config &config_;
    std::map<std::string, State> states_;
    std::vector<Frame> frames_;
};

} // namespace

void validate_dependencies(const Config &config) {
    Traversal traversal{config};
    for (const auto &[name, task] : config.tasks) {
        traversal.visit(name);
    }
}

std::vector<std::string> plan_task(const Config &config, std::string_view target) {
    const auto task = config.tasks.find(std::string{target});
    if (task == config.tasks.end()) {
        throw std::runtime_error("unknown task '" + std::string{target} + "'");
    }
    Traversal traversal{config};
    traversal.visit(task->first);
    return std::move(traversal.order);
}

} // namespace tatr
