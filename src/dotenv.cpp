#include "dotenv.hpp"

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace tatr {
namespace {

[[noreturn]] void invalid(const std::string &path, std::size_t line, const char *reason) {
    auto location = path;
    if (line != 0)
        location += ':' + std::to_string(line);
    throw std::runtime_error(location + ": " + reason);
}

bool space(char value) {
    return value == ' ' || value == '\t';
}

std::string_view left(std::string_view value) {
    while (!value.empty() && space(value.front()))
        value.remove_prefix(1);
    return value;
}

std::string_view right(std::string_view value) {
    while (!value.empty() && space(value.back()))
        value.remove_suffix(1);
    return value;
}

void validate_utf8(std::string_view source, const std::string &path) {
    std::size_t line = 1;
    for (std::size_t i = 0; i < source.size();) {
        const auto lead = static_cast<unsigned char>(source[i]);
        if (lead < 0x80) {
            if (lead == 0)
                invalid(path, line, "NUL is not allowed in an environment file");
            if (lead == '\n')
                ++line;
            ++i;
            continue;
        }
        std::size_t width = 0;
        std::uint32_t codepoint = 0;
        std::uint32_t minimum = 0;
        if (lead >= 0xc2 && lead <= 0xdf) {
            width = 2;
            codepoint = lead & 0x1fu;
            minimum = 0x80;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            width = 3;
            codepoint = lead & 0x0fu;
            minimum = 0x800;
        } else if (lead >= 0xf0 && lead <= 0xf4) {
            width = 4;
            codepoint = lead & 0x07u;
            minimum = 0x10000;
        } else {
            invalid(path, line, "environment file must contain valid UTF-8");
        }
        if (source.size() - i < width)
            invalid(path, line, "environment file must contain valid UTF-8");
        for (std::size_t j = 1; j < width; ++j) {
            const auto byte = static_cast<unsigned char>(source[i + j]);
            if (byte < 0x80 || byte > 0xbf)
                invalid(path, line, "environment file must contain valid UTF-8");
            codepoint = (codepoint << 6) | (byte & 0x3fu);
        }
        if (codepoint < minimum || codepoint > 0x10ffff ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            invalid(path, line, "environment file must contain valid UTF-8");
        }
        i += width;
    }
}

std::string parse_value(std::string_view value, const std::string &path, std::size_t line) {
    value = left(value);
    if (value.empty())
        return {};
    const auto quote = value.front();
    if (quote != '\'' && quote != '"') {
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (value[i] == '#' && (i == 0 || space(value[i - 1]))) {
                value = value.substr(0, i);
                break;
            }
        }
        return std::string{right(value)};
    }
    std::string result;
    for (std::size_t i = 1; i < value.size(); ++i) {
        const auto character = value[i];
        if (character == quote) {
            const auto tail = left(value.substr(i + 1));
            if (!tail.empty() && tail.front() != '#') {
                invalid(path, line, "unexpected text after quoted environment value");
            }
            return result;
        }
        if (quote == '"' && character == '\\') {
            if (++i == value.size())
                invalid(path, line, "unterminated quoted environment value");
            switch (value[i]) {
            case '\\':
                result += '\\';
                break;
            case '"':
                result += '"';
                break;
            case 'n':
                result += '\n';
                break;
            case 'r':
                result += '\r';
                break;
            case 't':
                result += '\t';
                break;
            default:
                invalid(path, line, "unsupported double-quote escape in environment value");
            }
        } else {
            result += character;
        }
    }
    invalid(path, line,
            "unterminated quoted environment value; physical multiline values are not supported");
}

} // namespace

EnvironmentOverrides load_dotenv(const std::filesystem::path &file) {
    const auto encoded = file.u8string();
    const std::string path{encoded.begin(), encoded.end()};
    std::error_code error;
    if (!std::filesystem::is_regular_file(file, error)) {
        invalid(path, 0, "cannot read environment file; expected a readable regular file");
    }
    std::ifstream input(file, std::ios::binary | std::ios::ate);
    if (!input)
        invalid(path, 0, "cannot open environment file");
    const auto length = input.tellg();
    if (length < 0)
        invalid(path, 0, "cannot read environment file size");
    constexpr std::streamoff max_bytes = 1024 * 1024;
    if (length > max_bytes)
        invalid(path, 0, "environment file exceeds 1 MiB limit (1048576 bytes)");
    std::string source(static_cast<std::size_t>(length), '\0');
    input.seekg(0);
    if (!input.read(source.data(), static_cast<std::streamsize>(source.size()))) {
        invalid(path, 0, "cannot read environment file");
    }
    if (input.peek() != std::ifstream::traits_type::eof()) {
        invalid(path, 0, "environment file changed while reading");
    }
    validate_utf8(source, path);
    std::string_view remaining{source};
    if (remaining.starts_with("\xef\xbb\xbf"))
        remaining.remove_prefix(3);
    EnvironmentOverrides result;
    std::size_t line = 0;
    while (!remaining.empty()) {
        ++line;
        const auto end = remaining.find('\n');
        auto text = remaining.substr(0, end);
        if (end == std::string_view::npos)
            remaining = {};
        else
            remaining.remove_prefix(end + 1);
        if (text.ends_with('\r')) {
            if (end == std::string_view::npos)
                invalid(path, line, "expected LF or CRLF line endings");
            text.remove_suffix(1);
        }
        if (text.find('\r') != std::string_view::npos)
            invalid(path, line, "expected LF or CRLF line endings");
        text = left(text);
        if (text.empty() || text.front() == '#')
            continue;
        if (text.starts_with("export") && text.size() > 6 && space(text[6])) {
            text = left(text.substr(6));
        }
        const auto separator = text.find('=');
        if (separator == std::string_view::npos)
            invalid(path, line, "expected NAME=value environment assignment");
        const auto name = right(text.substr(0, separator));
        if (!valid_environment_name(name))
            invalid(path, line, "environment variable names must match [A-Za-z_][A-Za-z0-9_]*");
        const auto key = environment_name_key(name);
        if (result.contains(key))
            invalid(path, line, "duplicate environment variable name");
        result.emplace(key, parse_value(text.substr(separator + 1), path, line));
    }
    return result;
}

} // namespace tatr
