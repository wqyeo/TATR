#include "process.hpp"

#include <limits>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <spawn.h>
#include <sys/wait.h>
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

std::wstring child_search_path(const std::filesystem::path &cwd) {
    const auto size = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    if (size == 0) {
        throw std::runtime_error("cannot launch process: inherited PATH is empty or unavailable");
    }
    std::wstring value(static_cast<std::size_t>(size), L'\0');
    const auto written = GetEnvironmentVariableW(L"PATH", value.data(), size);
    if (written == 0 || written >= size)
        windows_error("cannot read inherited PATH");
    value.resize(written);
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
                                const std::optional<std::filesystem::path> &cwd) {
    auto lookup = name;
    std::optional<std::wstring> search;
    if (cwd) {
        const std::filesystem::path executable{name};
        if (executable.has_root_name() && !executable.has_root_directory()) {
            throw std::runtime_error("cannot launch process: drive-relative executable paths are "
                                     "not supported with cwd");
        }
        if (executable.has_parent_path() || executable.has_root_name()) {
            lookup = (*cwd / executable).native();
        } else {
            search = child_search_path(*cwd);
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
    const posix_spawn_file_actions_t *get() const {
        return &value_;
    }

  private:
    posix_spawn_file_actions_t value_{};
};

} // namespace
#endif

ProcessResult run_process(std::span<const std::string> arguments,
                          const std::optional<std::filesystem::path> &cwd) {
    if (arguments.empty() || arguments.front().empty()) {
        throw std::runtime_error("process requires an executable name");
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
    const auto executable = resolve_executable(widen(arguments.front()), cwd);
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
    inherit_standard_handle(STD_INPUT_HANDLE, input);
    inherit_standard_handle(STD_OUTPUT_HANDLE, output);
    inherit_standard_handle(STD_ERROR_HANDLE, error);
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
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr,
                        inherited.empty() ? FALSE : TRUE, EXTENDED_STARTUPINFO_PRESENT, nullptr,
                        cwd ? cwd->c_str() : nullptr, &startup.StartupInfo, &information)) {
        windows_error("cannot launch process");
    }
    process.value = information.hProcess;
    thread.value = information.hThread;
    if (WaitForSingleObject(process.value, INFINITE) != WAIT_OBJECT_0) {
        windows_error("cannot wait for process");
    }
    DWORD status = 0;
    if (!GetExitCodeProcess(process.value, &status))
        windows_error("cannot read process status");
    return {static_cast<std::uint32_t>(status), 0};
#else
    std::vector<char *> argv;
    argv.reserve(arguments.size() + 1);
    for (const auto &argument : arguments)
        argv.push_back(const_cast<char *>(argument.c_str()));
    argv.push_back(nullptr);
    std::optional<FileActions> actions;
    if (cwd) {
        actions.emplace();
        actions->chdir(*cwd);
    }
    pid_t child = 0;
    const auto error = posix_spawnp(&child, argv[0], actions ? actions->get() : nullptr, nullptr,
                                    argv.data(), environ);
    if (error != 0) {
        throw std::system_error(error, std::generic_category(), "cannot launch process");
    }
    int status = 0;
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited == -1 && errno == EINTR);
    if (waited == -1) {
        throw std::system_error(errno, std::generic_category(), "cannot wait for process");
    }
    if (WIFSIGNALED(status))
        return {0, WTERMSIG(status)};
    if (WIFEXITED(status))
        return {static_cast<std::uint32_t>(WEXITSTATUS(status)), 0};
    throw std::runtime_error("unexpected process wait status");
#endif
}

} // namespace tatr
