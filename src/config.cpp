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
    check_fields(root, {"version", "tasks", "logging"}, path, "configuration");
    const auto version = toml_get(root, "version");
    if (version.type != TOML_INT64 || version.u.int64 != 1) {
        invalid(path, version, "version", "expected integer 1");
    }
    const auto tasks = toml_get(root, "tasks");
    if (tasks.type != TOML_TABLE) {
        invalid(path, tasks, "tasks", "expected a table");
    }
    Config config;
    const auto logging = toml_get(root, "logging");
    if (logging.type != TOML_UNKNOWN) {
        if (logging.type != TOML_TABLE) {
            invalid(path, logging, "logging", "expected a table");
        }
        check_fields(logging, {"level", "console", "file"}, path, "logging");
        const auto level = toml_get(logging, "level");
        if (level.type != TOML_UNKNOWN) {
            if (level.type != TOML_STRING) {
                invalid(path, level, "logging.level", "expected info, warn, error, or off");
            }
            const std::string_view text{level.u.str.ptr, static_cast<std::size_t>(level.u.str.len)};
            if (text == "info")
                config.logging.level = LogLevel::info;
            else if (text == "warn")
                config.logging.level = LogLevel::warn;
            else if (text == "error")
                config.logging.level = LogLevel::error;
            else if (text == "off")
                config.logging.level = LogLevel::off;
            else
                invalid(path, level, "logging.level", "expected info, warn, error, or off");
        }
        const auto console = toml_get(logging, "console");
        if (console.type != TOML_UNKNOWN) {
            if (console.type != TOML_BOOLEAN) {
                invalid(path, console, "logging.console", "expected a boolean");
            }
            config.logging.console = console.u.boolean;
        }
        const auto output = toml_get(logging, "file");
        if (output.type != TOML_UNKNOWN) {
            if (output.type != TOML_TABLE) {
                invalid(path, output, "logging.file", "expected a table");
            }
            check_fields(output, {"directory", "filename"}, path, "logging.file");
            config.logging.file.emplace();
            for (const auto key : {"directory", "filename"}) {
                const auto value = toml_get(output, key);
                if (value.type == TOML_UNKNOWN)
                    continue;
                const auto context = "logging.file." + std::string{key};
                if (value.type != TOML_STRING || value.u.str.len == 0) {
                    invalid(path, value, context, "expected a nonempty string");
                }
                const std::string text{value.u.str.ptr, static_cast<std::size_t>(value.u.str.len)};
                if (text.find('\0') != std::string::npos) {
                    invalid(path, value, context, "NUL characters are not allowed");
                }
                if (std::string_view{key} == "filename") {
                    config.logging.file->filename = text;
                } else {
                    config.logging.file->directory =
                        std::filesystem::path{std::u8string{text.begin(), text.end()}};
                }
            }
            auto &directory = config.logging.file->directory;
#ifdef _WIN32
            if (directory.has_root_name() && !directory.has_root_directory()) {
                invalid(path, output, "logging.file.directory",
                        "drive-relative paths are not supported");
            }
#endif
            directory = std::filesystem::absolute(file).parent_path() / directory;
            try {
                (void)log_filename(config.logging.file->filename,
                                   std::chrono::system_clock::time_point{}, 1);
            } catch (const std::runtime_error &error) {
                invalid(path, output, "logging.file", error.what());
            }
        }
        if (!config.logging.console && !config.logging.file &&
            config.logging.level != LogLevel::off) {
            invalid(path, logging, "logging", "console=false requires a file or level=off");
        }
    }
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
        check_fields(task, {"description", "depends", "exec", "cwd", "env", "env_file"}, path,
                     context);
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
        const auto cwd = toml_get(task, "cwd");
        if (cwd.type != TOML_UNKNOWN) {
            if (cwd.type != TOML_STRING || cwd.u.str.len == 0) {
                invalid(path, cwd, context + ".cwd", "expected a nonempty directory string");
            }
            const std::string directory{cwd.u.str.ptr, static_cast<std::size_t>(cwd.u.str.len)};
            if (directory.find('\0') != std::string::npos) {
                invalid(path, cwd, context + ".cwd", "NUL characters are not allowed");
            }
            if (model.exec.empty()) {
                invalid(path, cwd, context + ".cwd", "requires an executable task");
            }
            const std::filesystem::path selected{std::u8string{directory.begin(), directory.end()}};
#ifdef _WIN32
            if (selected.has_root_name() && !selected.has_root_directory()) {
                invalid(path, cwd, context + ".cwd", "drive-relative paths are not supported");
            }
#endif
            // Do not canonicalize: the directory may be created by a prerequisite,
            // and lexical normalization would change symlink/.. semantics.
            model.cwd = std::filesystem::absolute(file).parent_path() / selected;
        }
        const auto env = toml_get(task, "env");
        if (env.type != TOML_UNKNOWN) {
            if (env.type != TOML_TABLE) {
                invalid(path, env, context + ".env", "expected a table of string values");
            }
            if (model.exec.empty()) {
                invalid(path, env, context + ".env", "requires an executable task");
            }
            model.env.emplace();
            std::set<std::string> names;
            for (int j = 0; j < env.u.tab.size; ++j) {
                const std::string key{env.u.tab.key[j], static_cast<std::size_t>(env.u.tab.len[j])};
                const auto entry = env.u.tab.value[j];
                if (!valid_environment_name(key)) {
                    invalid(path, entry, context + ".env",
                            "variable names must match [A-Za-z_][A-Za-z0-9_]*");
                }
                if (!names.insert(environment_name_key(key)).second) {
                    invalid(path, entry, context + ".env",
                            "conflicting environment variable names");
                }
                if (entry.type != TOML_STRING) {
                    invalid(path, entry, context + ".env", "expected a string value");
                }
                std::string value{entry.u.str.ptr, static_cast<std::size_t>(entry.u.str.len)};
                if (value.find('\0') != std::string::npos) {
                    invalid(path, entry, context + ".env", "NUL values are not allowed");
                }
                model.env->emplace(key, std::move(value));
            }
        }
        const auto env_file = toml_get(task, "env_file");
        if (env_file.type != TOML_UNKNOWN) {
            if (env_file.type != TOML_STRING || env_file.u.str.len == 0) {
                invalid(path, env_file, context + ".env_file",
                        "expected a nonempty file-path string");
            }
            const std::string value{env_file.u.str.ptr,
                                    static_cast<std::size_t>(env_file.u.str.len)};
            if (value.find('\0') != std::string::npos) {
                invalid(path, env_file, context + ".env_file", "NUL characters are not allowed");
            }
            if (model.exec.empty()) {
                invalid(path, env_file, context + ".env_file", "requires an executable task");
            }
            const std::filesystem::path selected{std::u8string{value.begin(), value.end()}};
#ifdef _WIN32
            if (selected.has_root_name() && !selected.has_root_directory()) {
                invalid(path, env_file, context + ".env_file",
                        "drive-relative paths are not supported");
            }
#endif
            model.env_file = std::filesystem::absolute(file).parent_path() / selected;
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
