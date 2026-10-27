#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
std::filesystem::path path(const std::string &text) {
    return std::filesystem::path{std::u8string{text.begin(), text.end()}};
}
} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
#ifdef _WIN32
        const auto encoded = std::filesystem::path{argv[i]}.u8string();
        args.emplace_back(encoded.begin(), encoded.end());
#else
        args.emplace_back(argv[i]);
#endif
    }
    if (args.empty())
        return 90;
    const auto &mode = args[0];
    if (mode == "args") {
        constexpr char hex[] = "0123456789abcdef";
        for (std::size_t i = 1; i < args.size(); ++i) {
            std::cout << '[';
            for (const char character : args[i]) {
                const auto byte = static_cast<unsigned char>(character);
                std::cout << hex[byte >> 4] << hex[byte & 15];
            }
            std::cout << "]\n";
        }
    } else if (mode == "record" && args.size() == 3) {
        std::ofstream out(path(args[1]), std::ios::app);
        out << args[2] << '\n';
        if (!out)
            return 91;
    } else if (mode == "copy_file" && args.size() == 3) {
        std::filesystem::copy_file(path(args[1]), path(args[2]),
                                   std::filesystem::copy_options::overwrite_existing);
    } else if ((mode == "write_hex" || mode == "append_hex") && args.size() == 3) {
        const auto digit = [](char value) {
            if (value >= '0' && value <= '9')
                return value - '0';
            if (value >= 'a' && value <= 'f')
                return value - 'a' + 10;
            return -1;
        };
        if (args[2].size() % 2 != 0)
            return 98;
        std::ofstream out(path(args[1]),
                          std::ios::binary |
                              (mode == "append_hex" ? std::ios::app : std::ios::trunc));
        for (std::size_t i = 0; i < args[2].size(); i += 2) {
            const auto high = digit(args[2][i]);
            const auto low = digit(args[2][i + 1]);
            if (high < 0 || low < 0)
                return 99;
            out.put(static_cast<char>((high << 4) | low));
        }
        if (!out)
            return 100;
    } else if (mode == "mkdir" && args.size() == 2) {
        std::filesystem::create_directories(path(args[1]));
    } else if (mode == "io") {
        std::string line;
        std::getline(std::cin, line);
        std::cout << "stdout:" << line << '\n';
        std::cerr << "stderr:" << line << '\n';
    } else if (mode == "cwd") {
        const auto encoded = std::filesystem::current_path().u8string();
        std::cout << std::string{encoded.begin(), encoded.end()} << '\n';
    } else if (mode == "env_value" && args.size() == 2) {
#ifdef _WIN32
        const auto name = path(args[1]).native();
        SetLastError(ERROR_SUCCESS);
        const auto size = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
        if (size == 0) {
            if (GetLastError() == ERROR_ENVVAR_NOT_FOUND)
                std::cout << "missing\n";
            else if (GetLastError() == ERROR_SUCCESS)
                std::cout << "value:\n";
            else
                return 96;
        } else {
            std::wstring value(static_cast<std::size_t>(size), L'\0');
            const auto written = GetEnvironmentVariableW(name.c_str(), value.data(), size);
            if (written >= size)
                return 97;
            value.resize(written);
            const auto encoded = std::filesystem::path{value}.u8string();
            std::cout << "value:" << std::string{encoded.begin(), encoded.end()} << '\n';
        }
#else
        const auto *value = std::getenv(args[1].c_str());
        if (value)
            std::cout << "value:" << value << '\n';
        else
            std::cout << "missing\n";
#endif
    } else if (mode == "env") {
        const auto value = std::getenv("TATR_TEST_INHERITED");
        if (!value)
            return 92;
        std::cout << value << '\n';
    } else if (mode == "exit" && args.size() == 2) {
#ifdef _WIN32
        ExitProcess(static_cast<UINT>(std::stoul(args[1])));
#else
        return std::stoi(args[1]);
#endif
    }
#ifdef _WIN32
    else if (mode == "handle_excluded" && args.size() == 2) {
        const auto handle =
            reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(std::stoull(args[1])));
        // Signal the actual parent event if it leaked, avoiding handle-number
        // reuse false positives in the child's independent handle table.
        if (SetEvent(handle))
            return 95;
    }
#endif
    else if (mode == "signal") {
        std::raise(SIGTERM);
        return 93;
    } else {
        return 94;
    }
    return 0;
}
