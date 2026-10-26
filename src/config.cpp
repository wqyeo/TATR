#include "config.hpp"
#include "planner.hpp"

#include <tomlc17.h>

#include <algorithm>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace tatr {
namespace {

// Parsed datums borrow memory owned by this result. Copy only our model out.
struct Document {
    toml_result_t result;
    ~Document() {
        toml_free(result);
    }
    Document(const Document &) = delete;
    Document &operator=(const Document &) = delete;
    explicit Document(toml_result_t value) : result(value) {}
};

bool contains_control(std::string_view text) {
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto byte = static_cast<unsigned char>(text[i]);
        if (byte < 0x20 || byte == 0x7f) {
            return true;
        }
        // C1 controls, encoded as UTF-8. The parser validates UTF-8 itself.
        if (byte == 0xc2 && i + 1 < text.size()) {
            const auto next = static_cast<unsigned char>(text[i + 1]);
            if (next >= 0x80 && next <= 0x9f) {
                return true;
            }
        }
    }
    return false;
}

std::string display(std::string_view text) {
    // Keep malformed field names from inserting control characters into errors.
    if (contains_control(text)) {
        return "<name containing control characters>";
    }
    return "'" + std::string{text} + "'";
}

[[noreturn]] void invalid(const std::string &path, const toml_datum_t &value,
                          const std::string &field, const std::string &message) {
    std::string location = path;
    if (value.lineno > 0) {
        location += ':' + std::to_string(value.lineno) + ':' + std::to_string(value.colno);
    }
    throw std::runtime_error(location + ": " + field + ": " + message);
}

void check_fields(const toml_datum_t &table, std::initializer_list<std::string_view> allowed,
                  const std::string &path, const std::string &context) {
    for (int i = 0; i < table.u.tab.size; ++i) {
        const std::string_view key{table.u.tab.key[i],
                                   static_cast<std::size_t>(table.u.tab.len[i])};
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) {
            invalid(path, table.u.tab.value[i], context, "unsupported field " + display(key));
        }
    }
}

} // namespace

Config load_config(const std::filesystem::path &file) {
    const auto encoded_path = file.u8string();
    const std::string path{encoded_path.begin(), encoded_path.end()};
    // Use filesystem-aware I/O so Windows paths are not passed to a narrow fopen.
    std::ifstream input(file, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error(path + ": cannot open configuration file");
    }
    const auto length = input.tellg();
    if (length < 0) {
        throw std::runtime_error(path + ": cannot read configuration file size");
    }
    constexpr std::streamoff max_config_bytes = 1024 * 1024;
    if (length > max_config_bytes) {
        throw std::runtime_error(path + ": configuration exceeds 1 MiB limit (1048576 bytes)");
    }
    std::string source(static_cast<std::size_t>(length), '\0');
    input.seekg(0);
    if (!input.read(source.data(), static_cast<std::streamsize>(source.size()))) {
        throw std::runtime_error(path + ": cannot read configuration file");
    }
    const Document document{
        toml_parse_named(source.data(), static_cast<int>(source.size()), path.c_str())};
    if (!document.result.ok) {
        throw std::runtime_error(path + ": " + document.result.errmsg);
    }
    const auto root = document.result.toptab;
    check_fields(root, {"version", "tasks"}, path, "configuration");
    const auto version = toml_get(root, "version");
    if (version.type != TOML_INT64 || version.u.int64 != 1) {
        invalid(path, version, "version", "expected integer 1");
    }
    const auto tasks = toml_get(root, "tasks");
    if (tasks.type != TOML_TABLE) {
        invalid(path, tasks, "tasks", "expected a table");
    }
    Config config;
    for (int i = 0; i < tasks.u.tab.size; ++i) {
        const std::string name{tasks.u.tab.key[i], static_cast<std::size_t>(tasks.u.tab.len[i])};
        const auto task = tasks.u.tab.value[i];
        if (name.empty() || contains_control(name)) {
            invalid(path, task, "tasks",
                    "task names must be nonempty and contain no control characters");
        }
        const auto context = "task " + display(name);
        if (task.type != TOML_TABLE) {
            invalid(path, task, context, "expected a table");
        }
        check_fields(task, {"description", "depends", "exec"}, path, context);
        const auto description = toml_get(task, "description");
        Task model;
        if (description.type != TOML_UNKNOWN) {
            if (description.type != TOML_STRING) {
                invalid(path, description, context + ".description", "expected a string");
            }
            model.description.assign(description.u.str.ptr,
                                     static_cast<std::size_t>(description.u.str.len));
            if (contains_control(model.description)) {
                invalid(path, description, context + ".description",
                        "control characters are not allowed");
            }
        }
        const auto depends = toml_get(task, "depends");
        if (depends.type != TOML_UNKNOWN) {
            if (depends.type != TOML_ARRAY) {
                invalid(path, depends, context + ".depends",
                        "expected an array of task-name strings");
            }
            std::set<std::string> seen;
            for (int j = 0; j < depends.u.arr.size; ++j) {
                const auto entry = depends.u.arr.elem[j];
                if (entry.type != TOML_STRING) {
                    invalid(path, entry, context + ".depends", "expected a task-name string");
                }
                std::string dependency{entry.u.str.ptr, static_cast<std::size_t>(entry.u.str.len)};
                if (dependency.empty() || contains_control(dependency)) {
                    invalid(path, entry, context + ".depends",
                            "dependency names must be nonempty and contain no control characters");
                }
                if (!seen.insert(dependency).second) {
                    invalid(path, entry, context + ".depends",
                            "duplicate dependency " + display(dependency));
                }
                model.depends.push_back(std::move(dependency));
            }
        }
        const auto exec = toml_get(task, "exec");
        if (exec.type != TOML_UNKNOWN) {
            if (exec.type != TOML_ARRAY || exec.u.arr.size == 0) {
                invalid(path, exec, context + ".exec",
                        "expected a nonempty array of argument strings");
            }
            for (int j = 0; j < exec.u.arr.size; ++j) {
                const auto entry = exec.u.arr.elem[j];
                if (entry.type != TOML_STRING) {
                    invalid(path, entry, context + ".exec", "expected an argument string");
                }
                std::string argument{entry.u.str.ptr, static_cast<std::size_t>(entry.u.str.len)};
                if (argument.find('\0') != std::string::npos) {
                    invalid(path, entry, context + ".exec", "NUL characters are not allowed");
                }
                if (j == 0 && argument.empty()) {
                    invalid(path, entry, context + ".exec", "executable name must be nonempty");
                }
                model.exec.push_back(std::move(argument));
            }
        }
        config.tasks.emplace(name, std::move(model));
    }
    try {
        validate_dependencies(config);
    } catch (const std::runtime_error &error) {
        throw std::runtime_error(path + ": " + error.what());
    }
    return config;
}

} // namespace tatr
