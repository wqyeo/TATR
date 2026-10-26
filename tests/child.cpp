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
    } else if (mode == "io") {
        std::string line;
        std::getline(std::cin, line);
        std::cout << "stdout:" << line << '\n';
        std::cerr << "stderr:" << line << '\n';
    } else if (mode == "cwd") {
        const auto encoded = std::filesystem::current_path().u8string();
        std::cout << std::string{encoded.begin(), encoded.end()} << '\n';
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
