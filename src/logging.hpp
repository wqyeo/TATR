#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace tatr {

enum class LogLevel { info, warn, error, off };

class LogError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

struct LogFile {
    std::filesystem::path directory = "logs";
    std::string filename = "tatr-{date}-{time}-{pid}.log";
};

struct Logging {
    LogLevel level = LogLevel::info;
    bool console = true;
    std::optional<LogFile> file;
};

// Also validates the portable basename and template syntax; no filesystem I/O.
std::string log_filename(std::string_view pattern, std::chrono::system_clock::time_point time,
                         unsigned long pid);
std::string log_quote(std::string_view value);

struct LogRecord {
    LogLevel level;
    std::string_view event;
    std::string_view subject_key;
    std::string_view subject;
    std::string_view outcome = {};
    std::optional<unsigned long long> duration_ms = {};
    std::optional<unsigned long long> exit_code = {};
    std::optional<int> signal = {};
    std::string_view message = {};
};

class Logger {
  public:
    using Clock = std::function<std::chrono::system_clock::time_point()>;
    Logger(const Logging &settings, std::ostream &console,
           Clock clock = std::chrono::system_clock::now);
    // Borrowed stream seam: no filesystem access; streams must outlive the logger.
    Logger(LogLevel level, std::ostream &console, std::ostream &file, Clock clock);
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;
    Logger(Logger &&) = delete;
    Logger &operator=(Logger &&) = delete;
    void write(const LogRecord &record);
    void finish();
    bool enabled(LogLevel level) const;

  private:
    Logging settings_;
    std::ostream &console_;
    Clock clock_;
    std::ofstream file_;
    std::ostream *file_sink_ = nullptr;
};

} // namespace tatr
