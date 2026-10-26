#include "cli.hpp"
#include "runner.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
    try {
        std::vector<std::string> arguments;
        arguments.reserve(static_cast<std::size_t>(argc - 1));
        for (int i = 1; i < argc; ++i) {
#ifdef _WIN32
            // Native wide arguments preserve Unicode paths on Windows. u8string()
            // converts encoding without normalizing or rewriting the argument.
            const auto encoded = std::filesystem::path{argv[i]}.u8string();
            arguments.emplace_back(encoded.begin(), encoded.end());
#else
            arguments.emplace_back(argv[i]);
#endif
        }
        return tatr::run_cli(arguments, std::cout);
    } catch (const tatr::TaskFailure &error) {
        std::cerr << "tatr: " << error.what() << '\n';
        return 1;
    } catch (const std::exception &error) {
        std::cerr << "tatr: " << error.what() << '\n';
        return 2;
    }
}
