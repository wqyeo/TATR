#include "logging.hpp"
#include "cli.hpp"
#include "runner.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class Function> void fails(Function action) {
    bool failed = false;
    try {
        action();
    } catch (const std::runtime_error &) {
        failed = true;
    }
    require(failed, "expected checked failure");
}
struct Buffer : std::stringbuf {
    int fail_at;
    int flushes = 0;
    explicit Buffer(int index) : fail_at(index) {}
    int sync() override {
        return ++flushes == fail_at ? -1 : 0;
    }
};
} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
    try {
        using namespace std::chrono;
        const auto snapshot = system_clock::time_point{milliseconds{1793088000123LL}};
        const auto clock = [snapshot] { return snapshot; };
        require(tatr::log_filename("{date}-{time}-{pid}-{{ok}}.log", snapshot, 42) ==
                    "2026-10-27-080000123Z-42-{ok}.log",
                "UTC filename snapshot");
        for (const auto *name : {"",
                                 ".",
                                 "..",
                                 "../x",
                                 "a\\b",
                                 "a:b",
                                 "x?",
                                 "x*",
                                 "x|",
                                 "x<",
                                 "x>",
                                 "x\"",
                                 "x.",
                                 "x ",
                                 "NUL",
                                 "con.txt",
                                 "LPT9.log",
                                 "COM1",
                                 "COM\xc2\xb9.log",
                                 "x\n",
                                 "x\x7f",
                                 "x\xc2\x80",
                                 "{env}",
                                 "{",
                                 "}",
                                 "{date",
                                 "{date}}",
                                 "{{date}"}) {
            fails([&] { (void)tatr::log_filename(name, snapshot, 42); });
        }
        require(tatr::log_quote("a\n\r\t\"\\") == "\"a\\n\\r\\t\\\"\\\\\"", "quote escapes");
        require(tatr::log_quote(std::string{"\0\x01\x7f\xc2\x80\xff", 6}) ==
                    "\"\\x00\\x01\\x7f\\xc2\\x80\\xff\"",
                "control and invalid bytes");
        for (const auto *bad : {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82"}) {
            require(tatr::log_quote(bad).find("\\x") != std::string::npos, "invalid UTF-8 escaped");
        }
        require(tatr::log_quote("café 中文 😀") == "\"café 中文 😀\"", "UTF-8 preserved");
        std::ostringstream console;
        std::ostringstream file;
        tatr::Logger logger(tatr::LogLevel::info, console, file, clock);
        logger.write(
            {tatr::LogLevel::info, "task_finished", "task", "quoted\"task", "success", 7, 0});
        logger.finish();
        require(console.str() == file.str(), "sinks receive identical records");
        require(file.str() == "time=2026-10-27T08:00:00.123Z level=info event=task_finished "
                              "task=\"quoted\\\"task\" outcome=success exit_code=0 duration_ms=7\n",
                "record convention");
        for (const auto level :
             {tatr::LogLevel::warn, tatr::LogLevel::error, tatr::LogLevel::off}) {
            std::ostringstream sink;
            tatr::Logger filtered(tatr::Logging{level, true, {}}, sink, clock);
            filtered.write({tatr::LogLevel::info, "run_started", "target", "check"});
            require(sink.str().empty(), "threshold suppresses info");
            filtered.write({tatr::LogLevel::warn, "warning", "target", "check"});
            require(sink.str().empty() == (level != tatr::LogLevel::warn), "warn threshold");
        }
        for (const bool failing_file : {false, true}) {
            for (const int fail_at : {1, 2}) {
                Buffer buffer(fail_at);
                std::ostream broken(&buffer);
                std::ostringstream good;
                tatr::Logger failing(tatr::LogLevel::info, failing_file ? good : broken,
                                     failing_file ? broken : good, clock);
                fails([&] {
                    failing.write({tatr::LogLevel::info, "run_started", "target", "check"});
                    failing.finish();
                });
            }
        }
        for (const auto option : {"--help", "--version"}) {
            Buffer buffer(1);
            std::ostream broken(&buffer);
            std::ostringstream err;
            const std::string args[]{option};
            fails([&] { (void)tatr::run_cli(args, broken, err); });
        }
        // Failure on task_started must occur before even trying the invalid executable.
        tatr::Config config;
        config.tasks["check"].exec = {"tatr-executable-that-does-not-exist"};
        Buffer buffer(2);
        std::ostream broken(&buffer);
        bool sink_error = false;
        try {
            (void)tatr::run_task(config, "check", {}, broken);
        } catch (const tatr::LogError &) {
            sink_error = true;
        }
        require(sink_error && buffer.str().find("task_finished") == std::string::npos,
                "task_started failure stops launch without recursive logging");
        require(argc == 3, "expected child path and fixture directory");
        const auto encoded = std::filesystem::path{argv[1]}.u8string();
        const std::string child{encoded.begin(), encoded.end()};
        const auto root = std::filesystem::path{argv[2]};
        std::filesystem::create_directories(root);
        for (const int index : {2, 3, 4}) {
            const auto marker = root / (std::to_string(index) + ".txt");
            std::filesystem::remove(marker);
            const auto text = marker.u8string();
            const std::string marker_text{text.begin(), text.end()};
            tatr::Config chain;
            chain.tasks["first"].exec = {child, "record", marker_text, "first"};
            chain.tasks["last"].depends = {"first"};
            chain.tasks["last"].exec = {child, "record", marker_text, "last"};
            Buffer interrupted(index);
            std::ostream sink(&interrupted);
            bool failed = false;
            try {
                (void)tatr::run_task(chain, "last", {}, sink);
            } catch (const tatr::LogError &) {
                failed = true;
            }
            require(failed, "sink failure must escape as runner error");
            if (index == 2) {
                require(!std::filesystem::exists(marker), "failed start launched child");
            } else {
                std::ifstream input(marker);
                std::string line;
                std::getline(input, line);
                require(line == "first" && input.peek() == std::char_traits<char>::eof(),
                        "later child ran after sink failure");
            }
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
