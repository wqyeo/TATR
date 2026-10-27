#include "logging.hpp"

#include <array>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace tatr {
namespace {

std::string utc(std::chrono::system_clock::time_point time, const char *format) {
    const auto seconds = std::chrono::floor<std::chrono::seconds>(time);
    const auto native = std::chrono::system_clock::to_time_t(seconds);
    std::tm parts{};
#ifdef _WIN32
    if (gmtime_s(&parts, &native) != 0) {
#else
    if (gmtime_r(&native, &parts) == nullptr) {
#endif
        throw LogError("logging: cannot convert UTC time");
    }
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::put_time(&parts, format);
    return out.str();
}

std::string millis(std::chrono::system_clock::time_point time) {
    const auto value = std::chrono::duration_cast<std::chrono::milliseconds>(
                           time - std::chrono::floor<std::chrono::seconds>(time))
                           .count();
    auto digits = std::to_string(value);
    return std::string(3 - digits.size(), '0') + digits;
}

void basename(std::string_view name) {
    if (name.empty() || name == "." || name == ".." || name.back() == '.' || name.back() == ' ') {
        throw std::runtime_error("logging.file.filename: expected a portable file basename");
    }
    for (std::size_t i = 0; i < name.size(); ++i) {
        const auto byte = static_cast<unsigned char>(name[i]);
        if (byte < 32 || byte == 127 ||
            std::string_view{"<>:\"/\\|?*"}.find(name[i]) != std::string_view::npos ||
            (byte == 0xc2 && i + 1 < name.size() &&
             static_cast<unsigned char>(name[i + 1]) >= 0x80 &&
             static_cast<unsigned char>(name[i + 1]) <= 0x9f)) {
            throw std::runtime_error("logging.file.filename: forbidden filename character");
        }
    }
    std::string stem{name.substr(0, name.find('.'))};
    while (!stem.empty() && stem.back() == ' ') {
        stem.pop_back();
    }
    for (auto &ch : stem) {
        if (ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>(ch - 'a' + 'A');
        }
    }
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL" ||
        (stem.size() == 4 && (stem.starts_with("COM") || stem.starts_with("LPT")) &&
         stem[3] >= '1' && stem[3] <= '9') ||
        ((stem.starts_with("COM") || stem.starts_with("LPT")) &&
         (stem.substr(3) == "\xc2\xb9" || stem.substr(3) == "\xc2\xb2" ||
          stem.substr(3) == "\xc2\xb3"))) {
        throw std::runtime_error("logging.file.filename: reserved device name");
    }
}

std::string_view level_name(LogLevel level) {
    constexpr std::array names{"info", "warn", "error", "off"};
    return names.at(static_cast<std::size_t>(level));
}

} // namespace

std::string log_filename(std::string_view pattern, std::chrono::system_clock::time_point time,
                         unsigned long pid) {
    basename(pattern);
    std::string result;
    for (std::size_t i = 0; i < pattern.size();) {
        const char ch = pattern[i++];
        if (ch != '{' && ch != '}') {
            result += ch;
        } else if (i < pattern.size() && pattern[i] == ch) {
            result += ch;
            ++i;
        } else if (ch == '{') {
            const auto end = pattern.find('}', i);
            const auto token = pattern.substr(i, end == std::string_view::npos ? 0 : end - i);
            if (token == "date") {
                result += utc(time, "%Y-%m-%d");
            } else if (token == "time") {
                result += utc(time, "%H%M%S") + millis(time) + 'Z';
            } else if (token == "pid") {
                result += std::to_string(pid);
            } else {
                throw std::runtime_error(
                    "logging.file.filename: unknown token or malformed braces");
            }
            i = end + 1;
        } else {
            throw std::runtime_error("logging.file.filename: malformed braces");
        }
    }
    basename(result);
    return result;
}

std::string log_quote(std::string_view value) {
    constexpr std::string_view hex = "0123456789abcdef";
    std::string result = "\"";
    const auto escaped = [&](unsigned char byte) {
        result += "\\x";
        result += hex[byte >> 4];
        result += hex[byte & 15];
    };
    for (std::size_t i = 0; i < value.size();) {
        const auto byte = static_cast<unsigned char>(value[i]);
        if (byte < 0x80) {
            switch (byte) {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (byte < 32 || byte == 127)
                    escaped(byte);
                else
                    result += static_cast<char>(byte);
            }
            ++i;
            continue;
        }
        const std::size_t count = byte >= 0xc2 && byte <= 0xdf   ? 2
                                  : byte >= 0xe0 && byte <= 0xef ? 3
                                  : byte >= 0xf0 && byte <= 0xf4 ? 4
                                                                 : 0;
        bool valid = count != 0 && count <= value.size() - i;
        std::uint32_t point = byte & (count == 2 ? 0x1fu : count == 3 ? 0x0fu : 0x07u);
        for (std::size_t j = 1; valid && j < count; ++j) {
            const auto next = static_cast<unsigned char>(value[i + j]);
            valid = (next & 0xc0u) == 0x80u;
            point = (point << 6) | (next & 0x3fu);
        }
        valid = valid &&
                point >= (count == 2   ? 0x80u
                          : count == 3 ? 0x800u
                                       : 0x10000u) &&
                point <= 0x10ffff && !(point >= 0xd800 && point <= 0xdfff);
        if (!valid) {
            escaped(byte);
            ++i;
        } else if (point >= 0x80 && point <= 0x9f) {
            for (std::size_t j = 0; j < count; ++j)
                escaped(static_cast<unsigned char>(value[i++]));
        } else {
            result.append(value.substr(i, count));
            i += count;
        }
    }
    return result + '"';
}

Logger::Logger(const Logging &settings, std::ostream &console, Clock clock)
    : settings_(settings), console_(console), clock_(std::move(clock)) {
    if (settings_.file && settings_.level != LogLevel::off) {
        const auto snapshot = clock_();
#ifdef _WIN32
        const auto pid = static_cast<unsigned long>(_getpid());
#else
        const auto pid = static_cast<unsigned long>(getpid());
#endif
        const auto name = log_filename(settings_.file->filename, snapshot, pid);
        std::filesystem::create_directories(settings_.file->directory);
        const auto path = settings_.file->directory /
                          std::filesystem::path{std::u8string{name.begin(), name.end()}};
        file_.open(path, std::ios::binary | std::ios::app);
        if (!file_) {
            const auto encoded = path.u8string();
            throw LogError("logging: cannot open log file " +
                           log_quote(std::string{encoded.begin(), encoded.end()}));
        }
        file_sink_ = &file_;
    }
}

Logger::Logger(LogLevel level, std::ostream &console, std::ostream &file, Clock clock)
    : settings_{level, true, {}}, console_(console), clock_(std::move(clock)), file_sink_(&file) {}

bool Logger::enabled(LogLevel level) const {
    return level >= settings_.level && settings_.level != LogLevel::off;
}

void Logger::write(const LogRecord &record) try {
    if (!enabled(record.level))
        return;
    const auto now = clock_();
    std::string line = "time=" + utc(now, "%Y-%m-%dT%H:%M:%S") + '.' + millis(now) +
                       "Z level=" + std::string{level_name(record.level)} +
                       " event=" + std::string{record.event} + ' ' +
                       std::string{record.subject_key} + '=' + log_quote(record.subject);
    if (!record.outcome.empty())
        line += " outcome=" + std::string{record.outcome};
    if (record.exit_code)
        line += " exit_code=" + std::to_string(*record.exit_code);
    if (record.signal)
        line += " signal=" + std::to_string(*record.signal);
    if (record.duration_ms)
        line += " duration_ms=" + std::to_string(*record.duration_ms);
    if (!record.message.empty())
        line += " message=" + log_quote(record.message);
    line += '\n';
    if (settings_.console) {
        console_ << line << std::flush;
        if (!console_)
            throw LogError("logging: cannot write console record");
    }
    if (file_sink_ != nullptr) {
        *file_sink_ << line << std::flush;
        if (!*file_sink_)
            throw LogError("logging: cannot write log file record");
    }
} catch (const LogError &) {
    throw;
} catch (const std::exception &) {
    throw LogError("logging: cannot render or write record");
}

void Logger::finish() try {
    if (settings_.console && settings_.level != LogLevel::off) {
        console_.flush();
        if (!console_)
            throw LogError("logging: cannot finalize console output");
    }
    if (file_sink_ != nullptr) {
        file_sink_->flush();
        if (!*file_sink_)
            throw LogError("logging: cannot finalize log file");
    }
    if (file_.is_open()) {
        file_.flush();
        const bool flushed = static_cast<bool>(file_);
        file_.close();
        if (!flushed || !file_)
            throw LogError("logging: cannot finalize log file");
    }
} catch (const LogError &) {
    throw;
} catch (const std::exception &) {
    throw LogError("logging: cannot finalize output");
}

} // namespace tatr
