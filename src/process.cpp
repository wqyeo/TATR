#include "process.hpp"

#include <algorithm>
#include <cwchar>
#include <future>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace tatr {
#ifdef _WIN32
namespace {

[[noreturn]] void windows_error(const char *operation) {
    const auto error = GetLastError();
    throw std::runtime_error(std::string{operation} + " (Windows error " + std::to_string(error) +
                             ")");
}

struct Handle {
    HANDLE value = nullptr;
    ~Handle() {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
    void close() {
        const auto old = value;
        value = nullptr;
        if (old && old != INVALID_HANDLE_VALUE && !CloseHandle(old))
            windows_error("cannot close process I/O handle");
    }
    Handle() = default;
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
};

class Attributes {
  public:
    explicit Attributes(std::vector<HANDLE> &handles) {
        if (handles.empty())
            return;
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        if (size == 0)
            windows_error("cannot size process attributes");
        // operator new provides alignment suitable for the opaque native list.
        storage_.reset(::operator new(size));
        auto *list = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.get());
        if (!InitializeProcThreadAttributeList(list, 1, 0, &size)) {
            windows_error("cannot initialize process attributes");
        }
        list_.reset(list);
        if (!UpdateProcThreadAttribute(get(), 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles.data(),
                                       handles.size() * sizeof(HANDLE), nullptr, nullptr)) {
            windows_error("cannot restrict inherited handles");
        }
    }
    Attributes(const Attributes &) = delete;
    Attributes &operator=(const Attributes &) = delete;
    LPPROC_THREAD_ATTRIBUTE_LIST get() const {
        return static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(list_.get());
    }

  private:
    struct StorageDeleter {
        void operator()(void *value) const noexcept {
            ::operator delete(value);
        }
    };
    struct ListDeleter {
        void operator()(void *value) const noexcept {
            DeleteProcThreadAttributeList(static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(value));
        }
    };
    // Members are destroyed in reverse order: list cleanup precedes storage release.
    std::unique_ptr<void, StorageDeleter> storage_;
    std::unique_ptr<void, ListDeleter> list_;
};

std::wstring widen(const std::string &argument) {
    if (argument.empty())
        return {};
    if (argument.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("argument exceeds Windows size limit");
    }
    const auto length = static_cast<int>(argument.size());
    const auto size =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argument.data(), length, nullptr, 0);
    if (size == 0)
        windows_error("cannot encode process argument");
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argument.data(), length, result.data(),
                             size)) {
        windows_error("cannot encode process argument");
    }
    return result;
}

struct EnvironmentNameLess {
    bool operator()(const std::wstring &left, const std::wstring &right) const {
        const auto result = CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE);
        if (result == 0)
            windows_error("cannot compare environment names");
        return result == CSTR_LESS_THAN;
    }
};

class ChildEnvironment {
  public:
    explicit ChildEnvironment(const EnvironmentOverrides &overrides) {
        struct FreeStrings {
            void operator()(wchar_t *value) const noexcept {
                FreeEnvironmentStringsW(value);
            }
        };
        const std::unique_ptr<wchar_t, FreeStrings> native{GetEnvironmentStringsW()};
        if (!native)
            windows_error("cannot read inherited environment");
        for (const auto *entry = native.get(); *entry; entry += std::wcslen(entry) + 1) {
            const std::wstring text{entry};
            // Preserve hidden drive-current-directory entries such as =C:=C:\.
            const auto separator = text.find(L'=', text.front() == L'=' ? 1 : 0);
            if (separator == std::wstring::npos) {
                throw std::runtime_error("cannot read inherited environment entry");
            }
            values_.insert_or_assign(text.substr(0, separator), text.substr(separator + 1));
        }
        for (const auto &[key, value] : overrides) {
            auto name = widen(key);
            auto encoded = widen(value);
            if (name.size() + encoded.size() + 2 > 32767) {
                throw std::runtime_error("environment entry exceeds Windows size limit");
            }
            values_.insert_or_assign(std::move(name), std::move(encoded));
            if (environment_name_key(key) == "path")
                path_overridden_ = true;
        }
        for (const auto &[key, value] : values_) {
            block_.insert(block_.end(), key.begin(), key.end());
            block_.push_back(L'=');
            block_.insert(block_.end(), value.begin(), value.end());
            block_.push_back(L'\0');
        }
        if (block_.empty())
            block_.push_back(L'\0');
        block_.push_back(L'\0');
    }
    ChildEnvironment(const ChildEnvironment &) = delete;
    ChildEnvironment &operator=(const ChildEnvironment &) = delete;
    wchar_t *data() {
        return block_.data();
    }
    const std::wstring *override_path() const {
        if (!path_overridden_)
            return nullptr;
        return &values_.at(L"PATH");
    }

  private:
    std::map<std::wstring, std::wstring, EnvironmentNameLess> values_;
    std::vector<wchar_t> block_;
    bool path_overridden_ = false;
};

// Encode for the Microsoft CRT argv rules: double backslashes before quotes
// and before the closing quote. Windows passes one command-line string, not argv.
std::wstring quote(const std::wstring &argument) {
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return argument;
    }
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for (const auto character : argument) {
        if (character == L'\\') {
            ++slashes;
        } else {
            result.append(character == L'"' ? slashes * 2 + 1 : slashes, L'\\');
            result += character;
            slashes = 0;
        }
    }
    result.append(slashes * 2, L'\\');
    result += L'"';
    return result;
}

std::wstring child_search_path(const std::filesystem::path &cwd,
                               const std::wstring *effective_path) {
    std::wstring value;
    if (effective_path) {
        value = *effective_path;
    } else {
        const auto size = GetEnvironmentVariableW(L"PATH", nullptr, 0);
        if (size == 0) {
            throw std::runtime_error(
                "cannot launch process: inherited PATH is empty or unavailable");
        }
        value.resize(static_cast<std::size_t>(size));
        const auto written = GetEnvironmentVariableW(L"PATH", value.data(), size);
        if (written == 0 || written >= size)
            windows_error("cannot read inherited PATH");
        value.resize(written);
    }
    std::wstring result;
    std::size_t start = 0;
    do {
        const auto end = value.find(L';', start);
        auto entry = value.substr(start, end == std::wstring::npos ? end : end - start);
        if (entry.size() >= 2 && entry.front() == L'"' && entry.back() == L'"') {
            entry = entry.substr(1, entry.size() - 2);
        }
        const std::filesystem::path directory{entry};
        if (directory.has_root_name() && !directory.has_root_directory()) {
            throw std::runtime_error(
                "cannot launch process: drive-relative PATH entries are not supported with cwd");
        }
        if (!result.empty())
            result += L';';
        result += (entry.empty() ? cwd : cwd / directory).native();
        if (end == std::wstring::npos)
            break;
        start = end + 1;
    } while (true);
    return result;
}

std::wstring resolve_executable(const std::wstring &name,
                                const std::optional<std::filesystem::path> &cwd,
                                const std::wstring *effective_path) {
    auto lookup = name;
    std::optional<std::wstring> search;
    if (cwd || effective_path) {
        const auto directory = cwd ? *cwd : std::filesystem::current_path();
        const std::filesystem::path executable{name};
        if (executable.has_root_name() && !executable.has_root_directory()) {
            throw std::runtime_error("cannot launch process: drive-relative executable paths are "
                                     "not supported with cwd");
        }
        if (executable.has_parent_path() || executable.has_root_name()) {
            lookup = (directory / executable).native();
        } else {
            search = child_search_path(directory, effective_path);
        }
    }
    const auto *path = search ? search->c_str() : nullptr;
    const auto size = SearchPathW(path, lookup.c_str(), L".exe", 0, nullptr, nullptr);
    if (size == 0)
        windows_error("cannot launch process: executable lookup failed");
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    const auto written = SearchPathW(path, lookup.c_str(), L".exe", size, result.data(), nullptr);
    if (written == 0 || written >= size)
        windows_error("cannot launch process: executable lookup failed");
    result.resize(written);
    // Require a native executable image. In particular, never let Windows
    // implicitly dispatch .bat/.cmd files through a command interpreter.
    DWORD binary_type = 0;
    if (!GetBinaryTypeW(result.c_str(), &binary_type)) {
        windows_error("cannot launch process: expected a native executable; scripts require an "
                      "explicit interpreter");
    }
    return result;
}

void inherit_standard_handle(DWORD kind, Handle &destination) {
    const auto source = GetStdHandle(kind);
    if (source && source != INVALID_HANDLE_VALUE &&
        !DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), &destination.value, 0,
                         TRUE, DUPLICATE_SAME_ACCESS)) {
        windows_error("cannot inherit standard handle");
    }
}

} // namespace
#else
namespace {

struct Descriptor {
    int value = -1;
    ~Descriptor() {
        if (value >= 0)
            ::close(value);
    }
    Descriptor() = default;
    Descriptor(const Descriptor &) = delete;
    Descriptor &operator=(const Descriptor &) = delete;
    void adopt(int native) {
        value = native;
        if (native < 0)
            throw std::system_error(errno, std::generic_category(), "cannot open process I/O");
        if (native <= 2) {
            value = fcntl(native, F_DUPFD_CLOEXEC, 3);
            const auto saved = errno;
            ::close(native);
            if (value < 0)
                throw std::system_error(saved, std::generic_category(),
                                        "cannot duplicate process I/O");
        } else if (fcntl(value, F_SETFD, FD_CLOEXEC) < 0) {
            throw std::system_error(errno, std::generic_category(), "cannot restrict process I/O");
        }
    }
    void close() {
        const auto old = value;
        value = -1;
        if (old >= 0 && ::close(old) < 0)
            throw std::system_error(errno, std::generic_category(), "cannot close process I/O");
    }
};

class FileActions {
  public:
    FileActions() {
        const auto error = posix_spawn_file_actions_init(&value_);
        if (error != 0)
            throw std::system_error(error, std::generic_category(),
                                    "cannot initialize process actions");
    }
    ~FileActions() {
        posix_spawn_file_actions_destroy(&value_);
    }
    FileActions(const FileActions &) = delete;
    FileActions &operator=(const FileActions &) = delete;
    void chdir(const std::filesystem::path &directory) {
        const auto error = TATR_SPAWN_CHDIR(&value_, directory.c_str());
        if (error != 0)
            throw std::system_error(error, std::generic_category(),
                                    "cannot set child working directory");
    }
    void redirect(int source, int destination) {
        const auto error = posix_spawn_file_actions_adddup2(&value_, source, destination);
        if (error != 0)
            throw std::system_error(error, std::generic_category(), "cannot redirect process I/O");
    }
    void close(int descriptor) {
        const auto error = posix_spawn_file_actions_addclose(&value_, descriptor);
        if (error != 0)
            throw std::system_error(error, std::generic_category(), "cannot restrict process I/O");
    }
    const posix_spawn_file_actions_t *get() const {
        return &value_;
    }

  private:
    posix_spawn_file_actions_t value_{};
};

class ChildEnvironment {
  public:
    explicit ChildEnvironment(const EnvironmentOverrides &overrides) {
        for (auto *entry = environ; entry && *entry; ++entry) {
            const std::string_view text{*entry};
            const auto separator = text.find('=');
            if (separator != std::string_view::npos &&
                overrides.contains(std::string{text.substr(0, separator)}))
                continue;
            values_.emplace_back(text);
        }
        for (const auto &[key, value] : overrides) {
            values_.push_back(key + '=' + value);
        }
        pointers_.reserve(values_.size() + 1);
        for (auto &value : values_)
            pointers_.push_back(value.data());
        pointers_.push_back(nullptr);
    }
    ChildEnvironment(const ChildEnvironment &) = delete;
    ChildEnvironment &operator=(const ChildEnvironment &) = delete;
    char *const *data() const {
        return pointers_.data();
    }

  private:
    std::vector<std::string> values_;
    // Build pointers after all string mutations; the object cannot be copied/moved.
    std::vector<char *> pointers_;
};

int spawn_with_path(pid_t *child, const char *file, const posix_spawn_file_actions_t *actions,
                    char *const *argv, char *const *environment, const std::string &path) {
    if (std::string_view{file}.find('/') != std::string_view::npos) {
        return posix_spawn(child, file, actions, nullptr, argv, environment);
    }
    bool denied = false;
    std::size_t start = 0;
    do {
        const auto end = path.find(':', start);
        auto candidate = path.substr(start, end == std::string::npos ? end : end - start);
        if (!candidate.empty())
            candidate += '/';
        candidate += file;
        // Each relative candidate is executed after the child-only chdir action.
        // Do not use access() preflights or fall back to a shell on ENOEXEC.
        const auto error =
            posix_spawn(child, candidate.c_str(), actions, nullptr, argv, environment);
        if (error == 0)
            return 0;
        if (error == EACCES)
            denied = true;
        else if (error != ENOENT && error != ENOTDIR)
            return error;
        if (end == std::string::npos)
            break;
        start = end + 1;
    } while (true);
    return denied ? EACCES : ENOENT;
}

} // namespace
#endif

ProcessResult run_process(std::span<const std::string> arguments,
                          const std::optional<std::filesystem::path> &cwd,
                          const std::optional<EnvironmentOverrides> &env,
                          const std::optional<ProcessInput> &supplied_input) {
    if (arguments.empty() || arguments.front().empty()) {
        throw std::runtime_error("process requires an executable name");
    }
    if (env) {
        std::set<std::string> names;
        for (const auto &[key, value] : *env) {
            if (!valid_environment_name(key) || value.find('\0') != std::string::npos ||
                !names.insert(environment_name_key(key)).second) {
                throw std::runtime_error("invalid environment overrides");
            }
        }
    }
    for (const auto &argument : arguments) {
        if (argument.find('\0') != std::string::npos) {
            throw std::runtime_error("process arguments cannot contain NUL");
        }
    }
    if (cwd) {
        if (!cwd->is_absolute() || cwd->native().find(std::filesystem::path::value_type{}) !=
                                       std::filesystem::path::string_type::npos) {
            throw std::runtime_error("working directory must be absolute and contain no NUL");
        }
        if (!std::filesystem::is_directory(*cwd)) {
            throw std::runtime_error("working directory does not exist or is not a directory");
        }
    }
#ifdef _WIN32
    std::optional<ChildEnvironment> environment;
    if (env)
        environment.emplace(*env);
    const auto executable = resolve_executable(
        widen(arguments.front()), cwd, environment ? environment->override_path() : nullptr);
    std::wstring command;
    for (const auto &argument : arguments) {
        if (!command.empty())
            command += L' ';
        command += quote(widen(argument));
    }
    if (command.size() >= 32767) {
        throw std::runtime_error("command line exceeds Windows size limit");
    }
    Handle input, output, error, process, thread;
    Handle pipe_write;
    if (supplied_input) {
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        if (!CreatePipe(&input.value, &pipe_write.value, &security, 0) ||
            !SetHandleInformation(pipe_write.value, HANDLE_FLAG_INHERIT, 0))
            windows_error("cannot create process input pipe");
        if (supplied_input->quiet) {
            output.value = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            error.value = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (output.value == INVALID_HANDLE_VALUE || error.value == INVALID_HANDLE_VALUE)
                windows_error("cannot open null output");
        } else {
            inherit_standard_handle(STD_OUTPUT_HANDLE, output);
            inherit_standard_handle(STD_ERROR_HANDLE, error);
        }
    } else {
        inherit_standard_handle(STD_INPUT_HANDLE, input);
        inherit_standard_handle(STD_OUTPUT_HANDLE, output);
        inherit_standard_handle(STD_ERROR_HANDLE, error);
    }
    std::vector<HANDLE> inherited;
    for (const auto handle : {input.value, output.value, error.value}) {
        if (handle)
            inherited.push_back(handle);
    }
    Attributes attributes{inherited};
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input.value;
    startup.StartupInfo.hStdOutput = output.value;
    startup.StartupInfo.hStdError = error.value;
    startup.lpAttributeList = attributes.get();
    PROCESS_INFORMATION information{};
    int write_error = 0;
    std::jthread writer;
    // Destroying an unreleased promise wakes the writer before its joiner runs.
    std::optional<std::promise<bool>> start_writer;
    if (supplied_input) {
        start_writer.emplace();
        writer = std::jthread([&, ready = start_writer->get_future()]() mutable {
            bool enabled = false;
            try {
                enabled = ready.get();
            } catch (const std::future_error &) {
            }
            std::size_t offset = 0;
            while (enabled && offset < supplied_input->bytes.size()) {
                DWORD written = 0;
                const auto count = static_cast<DWORD>(
                    std::min<std::size_t>(4096, supplied_input->bytes.size() - offset));
                if (!WriteFile(pipe_write.value, supplied_input->bytes.data() + offset, count,
                               &written, nullptr) ||
                    written == 0) {
                    write_error = static_cast<int>(GetLastError());
                    if (write_error == 0)
                        write_error = ERROR_WRITE_FAULT;
                    break;
                }
                offset += written;
            }
            if (!CloseHandle(pipe_write.value) && write_error == 0)
                write_error = static_cast<int>(GetLastError());
            pipe_write.value = nullptr;
        });
    }
    if (!CreateProcessW(
            executable.c_str(), command.data(), nullptr, nullptr, inherited.empty() ? FALSE : TRUE,
            EXTENDED_STARTUPINFO_PRESENT | (environment ? CREATE_UNICODE_ENVIRONMENT : 0),
            environment ? environment->data() : nullptr, cwd ? cwd->c_str() : nullptr,
            &startup.StartupInfo, &information)) {
        const auto saved = GetLastError();
        if (supplied_input) {
            // Close the last reader so a writer blocked on a full pipe can finish.
            start_writer->set_value(false);
            input.close();
            writer.join();
        }
        SetLastError(saved);
        windows_error("cannot launch process");
    }
    process.value = information.hProcess;
    thread.value = information.hThread;
    if (supplied_input) {
        try {
            input.close();
        } catch (...) {
            start_writer->set_value(false);
            TerminateProcess(process.value, 2);
            WaitForSingleObject(process.value, INFINITE);
            writer.join();
            throw;
        }
    }
    if (supplied_input)
        start_writer->set_value(true);
    if (WaitForSingleObject(process.value, INFINITE) != WAIT_OBJECT_0) {
        const auto saved = GetLastError();
        TerminateProcess(process.value, 2);
        WaitForSingleObject(process.value, INFINITE);
        if (writer.joinable())
            writer.join();
        SetLastError(saved);
        windows_error("cannot wait for process");
    }
    if (writer.joinable())
        writer.join();
    DWORD status = 0;
    if (!GetExitCodeProcess(process.value, &status))
        windows_error("cannot read process status");
    if (write_error != 0 &&
        (status == 0 || (write_error != ERROR_BROKEN_PIPE && write_error != ERROR_NO_DATA)))
        throw std::runtime_error("cannot supply process input (Windows error " +
                                 std::to_string(write_error) + ")");
    if (supplied_input && supplied_input->quiet) {
        output.close();
        error.close();
    }
    return {static_cast<std::uint32_t>(status), 0};
#else
    std::vector<char *> argv;
    argv.reserve(arguments.size() + 1);
    for (const auto &argument : arguments)
        argv.push_back(const_cast<char *>(argument.c_str()));
    argv.push_back(nullptr);
    std::optional<FileActions> actions;
    if (cwd || supplied_input)
        actions.emplace();
    if (cwd)
        actions->chdir(*cwd);
    Descriptor input, output, pipe_write;
    if (supplied_input) {
        int descriptors[2];
        if (pipe(descriptors) < 0)
            throw std::system_error(errno, std::generic_category(),
                                    "cannot create process input pipe");
        // Take ownership of both descriptors before any fallible setup.
        input.value = descriptors[0];
        pipe_write.value = descriptors[1];
        input.adopt(input.value);
        pipe_write.adopt(pipe_write.value);
        actions->redirect(input.value, STDIN_FILENO);
        actions->close(input.value);
        actions->close(pipe_write.value);
        if (supplied_input->quiet) {
            output.adopt(open("/dev/null", O_WRONLY | O_CLOEXEC));
            actions->redirect(output.value, STDOUT_FILENO);
            actions->redirect(output.value, STDERR_FILENO);
            actions->close(output.value);
        }
    }
    pid_t child = 0;
    std::optional<ChildEnvironment> environment;
    if (env)
        environment.emplace(*env);
    auto *const *native_environment = environment ? environment->data() : environ;
    const auto *file_actions = actions ? actions->get() : nullptr;
    const std::string *child_path = nullptr;
    if (env) {
        const auto found = env->find("PATH");
        if (found != env->end())
            child_path = &found->second;
    }
    int write_error = 0;
    std::jthread writer;
    // Destroying an unreleased promise wakes the writer before its joiner runs.
    std::optional<std::promise<bool>> start_writer;
    if (supplied_input) {
        start_writer.emplace();
        writer = std::jthread([&, ready = start_writer->get_future()]() mutable {
            bool enabled = false;
            try {
                enabled = ready.get();
            } catch (const std::future_error &) {
            }
            // This newly-created thread keeps SIGPIPE blocked until it exits;
            // pending thread-directed SIGPIPE is discarded on thread termination.
            sigset_t blocked;
            sigemptyset(&blocked);
            sigaddset(&blocked, SIGPIPE);
            write_error = pthread_sigmask(SIG_BLOCK, &blocked, nullptr);
            std::size_t offset = 0;
            while (enabled && write_error == 0 && offset < supplied_input->bytes.size()) {
                const auto count =
                    write(pipe_write.value, supplied_input->bytes.data() + offset,
                          std::min<std::size_t>(4096, supplied_input->bytes.size() - offset));
                if (count < 0 && errno == EINTR)
                    continue;
                if (count <= 0) {
                    write_error = count < 0 ? errno : EIO;
                    break;
                }
                offset += static_cast<std::size_t>(count);
            }
            if (::close(pipe_write.value) < 0 && write_error == 0)
                write_error = errno;
            pipe_write.value = -1;
        });
    }
    const auto error = child_path ? spawn_with_path(&child, argv[0], file_actions, argv.data(),
                                                    native_environment, *child_path)
                                  : posix_spawnp(&child, argv[0], file_actions, nullptr,
                                                 argv.data(), native_environment);
    if (error != 0) {
        if (supplied_input) {
            start_writer->set_value(false);
            input.close();
            writer.join();
        }
        throw std::system_error(error, std::generic_category(), "cannot launch process");
    }
    if (supplied_input) {
        try {
            input.close();
        } catch (...) {
            start_writer->set_value(false);
            kill(child, SIGKILL);
            waitpid(child, nullptr, 0);
            writer.join();
            throw;
        }
    }
    if (supplied_input)
        start_writer->set_value(true);
    int status = 0;
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited == -1 && errno == EINTR);
    if (waited == -1) {
        const auto saved = errno;
        kill(child, SIGKILL);
        while (waitpid(child, nullptr, 0) < 0 && errno == EINTR) {
        }
        if (writer.joinable())
            writer.join();
        throw std::system_error(saved, std::generic_category(), "cannot wait for process");
    }
    if (writer.joinable())
        writer.join();
    if (write_error != 0 &&
        ((WIFEXITED(status) && WEXITSTATUS(status) == 0) || write_error != EPIPE))
        throw std::runtime_error("cannot supply process input");
    if (supplied_input && supplied_input->quiet)
        output.close();
    if (WIFSIGNALED(status))
        return {0, WTERMSIG(status)};
    if (WIFEXITED(status))
        return {static_cast<std::uint32_t>(WEXITSTATUS(status)), 0};
    throw std::runtime_error("unexpected process wait status");
#endif
}

} // namespace tatr
