#include "smtp.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <locale>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <aclapi.h>
#include <sddl.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace tatr {
namespace {
[[noreturn]] void bad(const char *reason) {
    throw std::runtime_error(std::string{"SMTP: "} + reason);
}

void text(std::string_view value, bool controls) {
    for (std::size_t i = 0; i < value.size();) {
        const auto byte = static_cast<unsigned char>(value[i]);
        if (byte == 0)
            bad("NUL is not allowed");
        if (byte < 0x80) {
            if (controls && (byte < 32 || byte == 127))
                bad("control characters are not allowed");
            ++i;
            continue;
        }
        const std::size_t count = byte >= 0xc2 && byte <= 0xdf   ? 2
                                  : byte >= 0xe0 && byte <= 0xef ? 3
                                  : byte >= 0xf0 && byte <= 0xf4 ? 4
                                                                 : 0;
        if (count == 0 || count > value.size() - i)
            bad("expected valid UTF-8");
        std::uint32_t point = byte & (count == 2 ? 0x1fu : count == 3 ? 0x0fu : 0x07u);
        for (std::size_t j = 1; j < count; ++j) {
            const auto next = static_cast<unsigned char>(value[i + j]);
            if ((next & 0xc0u) != 0x80u)
                bad("expected valid UTF-8");
            point = (point << 6) | (next & 0x3fu);
        }
        if (point < (count == 2   ? 0x80u
                     : count == 3 ? 0x800u
                                  : 0x10000u) ||
            point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff))
            bad("expected valid UTF-8");
        if (controls && point >= 0x80 && point <= 0x9f)
            bad("control characters are not allowed");
        i += count;
    }
}

bool alnum(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
}
bool dns(std::string_view value) {
    if (value.empty() || value.size() > 253)
        return false;
    if (value.back() == '.')
        value.remove_suffix(1);
    while (!value.empty()) {
        const auto dot = value.find('.');
        const auto label = value.substr(0, dot);
        if (label.empty() || label.size() > 63 || !alnum(label.front()) || !alnum(label.back()))
            return false;
        for (const char ch : label)
            if (!alnum(ch) && ch != '-')
                return false;
        if (dot == std::string_view::npos)
            return true;
        value.remove_prefix(dot + 1);
        if (value.empty())
            return false;
    }
    return false;
}
bool ipv4(std::string_view value) {
    unsigned parts = 0;
    while (!value.empty()) {
        const auto dot = value.find('.');
        const auto part = value.substr(0, dot);
        unsigned number = 0;
        const auto parsed = std::from_chars(part.data(), part.data() + part.size(), number);
        if (part.empty() || (part.size() > 1 && part.front() == '0') || parsed.ec != std::errc{} ||
            parsed.ptr != part.data() + part.size() || number > 255)
            return false;
        ++parts;
        if (dot == std::string_view::npos)
            return parts == 4;
        value.remove_prefix(dot + 1);
    }
    return false;
}
bool ipv6(std::string_view value) {
    if (value.size() < 2 || value.front() != '[' || value.back() != ']')
        return false;
    value.remove_prefix(1);
    value.remove_suffix(1);
    const auto compressed = value.find("::");
    if (compressed != std::string_view::npos &&
        value.find("::", compressed + 2) != std::string_view::npos)
        return false;
    unsigned count = 0;
    const auto groups = [&](std::string_view side, bool last) {
        while (!side.empty()) {
            const auto colon = side.find(':');
            const auto group = side.substr(0, colon);
            if (group.find('.') != std::string_view::npos) {
                if (!last || colon != std::string_view::npos || !ipv4(group))
                    return false;
                count += 2;
            } else {
                unsigned number = 0;
                const auto parsed =
                    std::from_chars(group.data(), group.data() + group.size(), number, 16);
                if (group.empty() || group.size() > 4 || parsed.ec != std::errc{} ||
                    parsed.ptr != group.data() + group.size())
                    return false;
                ++count;
            }
            if (colon == std::string_view::npos)
                return true;
            side.remove_prefix(colon + 1);
            if (side.empty())
                return false;
        }
        return true;
    };
    if (compressed == std::string_view::npos)
        return !value.empty() && groups(value, true) && count == 8;
    return groups(value.substr(0, compressed), false) &&
           groups(value.substr(compressed + 2), true) && count < 8;
}
bool mailbox(std::string_view value) {
    if (value.size() > 254)
        return false;
    const auto at = value.find('@');
    if (at == 0 || at == std::string_view::npos || at > 64 ||
        value.find('@', at + 1) != std::string_view::npos)
        return false;
    const auto local = value.substr(0, at);
    if (local.front() == '.' || local.back() == '.' || local.find("..") != std::string_view::npos)
        return false;
    for (const char ch : local) {
        if (!alnum(ch) &&
            std::string_view{".!#$%&'*+-/=?^_`{|}~"}.find(ch) == std::string_view::npos)
            return false;
    }
    const auto domain = value.substr(at + 1);
    return !domain.empty() && domain.back() != '.' && dns(domain);
}
std::string normalize_body(std::string_view body) {
    if (body.size() > 65536)
        bad("body exceeds 64 KiB limit");
    text(body, false);
    std::string result;
    for (std::size_t i = 0; i < body.size(); ++i) {
        if (body[i] == '\r') {
            if (i + 1 == body.size() || body[i + 1] != '\n')
                bad("body contains a lone CR");
            continue;
        }
        result += body[i];
    }
    return result;
}
std::string encoded_body(std::string_view body) {
    constexpr std::string_view hex = "0123456789ABCDEF";
    std::string result;
    unsigned column = 0;
    for (const char character : body) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte == '\n') {
            result += "\r\n";
            column = 0;
            continue;
        }
        const bool literal = (byte >= 33 && byte <= 60) || (byte >= 62 && byte <= 126);
        const unsigned width = literal ? 1u : 3u;
        if (column + width > 72) {
            result += "=\r\n";
            column = 0;
        }
        if (literal)
            result += static_cast<char>(byte);
        else {
            result += '=';
            result += hex[byte >> 4];
            result += hex[byte & 15];
        }
        column += width;
    }
    if (!result.ends_with("\r\n"))
        result += "\r\n";
    return result;
}
std::string base64(std::string_view input) {
    constexpr std::string_view alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < input.size(); i += 3) {
        const auto a = static_cast<unsigned char>(input[i]);
        const auto b = i + 1 < input.size() ? static_cast<unsigned char>(input[i + 1]) : 0u;
        const auto c = i + 2 < input.size() ? static_cast<unsigned char>(input[i + 2]) : 0u;
        const auto bits = (static_cast<unsigned>(a) << 16) | (b << 8) | c;
        out += alphabet[(bits >> 18) & 63];
        out += alphabet[(bits >> 12) & 63];
        out += i + 1 < input.size() ? alphabet[(bits >> 6) & 63] : '=';
        out += i + 2 < input.size() ? alphabet[bits & 63] : '=';
    }
    return out;
}
std::string subject_header(std::string_view subject) {
    std::string out = "Subject: ";
    while (!subject.empty()) {
        auto count = std::min<std::size_t>(42, subject.size());
        while (count < subject.size() &&
               (static_cast<unsigned char>(subject[count]) & 0xc0u) == 0x80u)
            --count;
        out += "=?UTF-8?B?" + base64(subject.substr(0, count)) + "?=";
        subject.remove_prefix(count);
        if (!subject.empty())
            out += "\r\n ";
    }
    return out + "\r\n";
}
std::string quoted(std::string_view value) {
    if (value.find_first_of("\r\n\0", 0, 3) != std::string_view::npos)
        bad("invalid client configuration value");
    std::string out = "\"";
    for (const char ch : value) {
        if (ch == '\\' || ch == '"')
            out += '\\';
        out += ch;
    }
    return out + "\"";
}
std::string path_text(const std::filesystem::path &path) {
    const auto utf8 = path.u8string();
    return {utf8.begin(), utf8.end()};
}
std::string random_name() {
    std::random_device random;
    constexpr std::string_view hex = "0123456789abcdef";
    std::string out;
    for (unsigned i = 0; i < 4; ++i) {
        const auto number = random();
        for (unsigned shift = 0; shift < 32; shift += 4)
            out += hex[(number >> shift) & 15u];
    }
    return out;
}

#ifdef _WIN32
struct LocalMemory {
    void *value = nullptr;
    ~LocalMemory() {
        if (value)
            LocalFree(value);
    }
    LocalMemory() = default;
    LocalMemory(const LocalMemory &) = delete;
    LocalMemory &operator=(const LocalMemory &) = delete;
};
struct NativeFile {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~NativeFile() {
        if (value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
    NativeFile() = default;
    NativeFile(const NativeFile &) = delete;
    NativeFile &operator=(const NativeFile &) = delete;
};
class PrivateSecurity {
  public:
    PrivateSecurity() {
        NativeFile token;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.value))
            bad("cannot prepare private file access");
        DWORD size = 0;
        GetTokenInformation(token.value, TokenUser, nullptr, 0, &size);
        if (size == 0)
            bad("cannot size private file access");
        std::vector<std::byte> data(size);
        if (!GetTokenInformation(token.value, TokenUser, data.data(), size, &size))
            bad("cannot read private file access");
        LPWSTR sid = nullptr;
        const auto *user = reinterpret_cast<const TOKEN_USER *>(data.data());
        if (!ConvertSidToStringSidW(user->User.Sid, &sid))
            bad("cannot encode private file access");
        LocalMemory sid_memory;
        sid_memory.value = sid;
        const auto sddl = L"D:P(A;OICI;FA;;;" + std::wstring{sid} + L")";
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
                sddl.c_str(), SDDL_REVISION_1,
                reinterpret_cast<PSECURITY_DESCRIPTOR *>(&descriptor_.value), nullptr))
            bad("cannot create private file access");
        attributes_.nLength = sizeof(attributes_);
        attributes_.lpSecurityDescriptor = descriptor_.value;
    }
    SECURITY_ATTRIBUTES *get() {
        return &attributes_;
    }
    void verify(const std::filesystem::path &directory) {
        LocalMemory actual;
        PACL acl = nullptr;
        if (GetNamedSecurityInfoW(directory.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                  nullptr, nullptr, &acl, nullptr,
                                  reinterpret_cast<PSECURITY_DESCRIPTOR *>(&actual.value)) !=
                ERROR_SUCCESS ||
            !acl || acl->AceCount != 1)
            bad("temporary filesystem does not provide private access controls");
        SECURITY_DESCRIPTOR_CONTROL control = 0;
        DWORD revision = 0;
        if (!GetSecurityDescriptorControl(actual.value, &control, &revision) ||
            !(control & SE_DACL_PROTECTED))
            bad("temporary directory access controls are not protected");
        BOOL present = FALSE, defaulted = FALSE;
        PACL expected = nullptr;
        if (!GetSecurityDescriptorDacl(descriptor_.value, &present, &expected, &defaulted) ||
            !present || !expected)
            bad("cannot verify private access controls");
        void *actual_ace = nullptr, *expected_ace = nullptr;
        if (!GetAce(acl, 0, &actual_ace) || !GetAce(expected, 0, &expected_ace))
            bad("cannot inspect private access controls");
        const auto *actual_access = static_cast<const ACCESS_ALLOWED_ACE *>(actual_ace);
        const auto *expected_access = static_cast<const ACCESS_ALLOWED_ACE *>(expected_ace);
        if (actual_access->Header.AceType != ACCESS_ALLOWED_ACE_TYPE ||
            actual_access->Mask != FILE_ALL_ACCESS ||
            !EqualSid(const_cast<DWORD *>(&actual_access->SidStart),
                      const_cast<DWORD *>(&expected_access->SidStart)))
            bad("temporary filesystem does not provide private access controls");
    }

  private:
    LocalMemory descriptor_;
    SECURITY_ATTRIBUTES attributes_{};
};
#endif

class MessageFile {
  public:
    MessageFile() {
        const auto root = std::filesystem::temp_directory_path();
#ifdef _WIN32
        PrivateSecurity security;
        for (unsigned attempt = 0; attempt < 64; ++attempt) {
            const auto candidate = root / ("tatr-smtp-" + random_name());
            if (CreateDirectoryW(candidate.c_str(), security.get())) {
                directory_ = candidate;
                break;
            }
            if (GetLastError() != ERROR_ALREADY_EXISTS)
                bad("cannot create private message directory");
        }
#else
        auto pattern = path_text(root / "tatr-smtp-XXXXXX");
        const auto *created = mkdtemp(pattern.data());
        if (!created)
            bad("cannot create private message directory");
        directory_ = std::filesystem::path{created};
        struct stat information{};
        if (stat(directory_.c_str(), &information) != 0 || (information.st_mode & 0777) != 0700) {
            rmdir(directory_.c_str());
            bad("temporary filesystem does not provide private directory permissions");
        }
#endif
        if (directory_.empty())
            bad("cannot create unique message directory");
#ifdef _WIN32
        try {
            security.verify(directory_);
        } catch (...) {
            RemoveDirectoryW(directory_.c_str());
            throw;
        }
#endif
    }
    ~MessageFile() {
        std::error_code ignored;
        if (!file_.empty())
            std::filesystem::remove(file_, ignored);
        if (!directory_.empty())
            std::filesystem::remove(directory_, ignored);
    }
    MessageFile(const MessageFile &) = delete;
    MessageFile &operator=(const MessageFile &) = delete;
    std::filesystem::path write(std::string_view value) {
        const auto selected = directory_ / "message.eml";
#ifdef _WIN32
        NativeFile file;
        file.value = CreateFileW(selected.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                 FILE_ATTRIBUTE_TEMPORARY, nullptr);
        if (file.value == INVALID_HANDLE_VALUE)
            bad("cannot create private message file");
        file_ = selected;
        DWORD written = 0;
        if (!WriteFile(file.value, value.data(), static_cast<DWORD>(value.size()), &written,
                       nullptr) ||
            written != value.size())
            bad("cannot write private message file");
        const auto native = file.value;
        file.value = INVALID_HANDLE_VALUE;
        if (!CloseHandle(native))
            bad("cannot close private message file");
#else
        const auto native =
            open(selected.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, S_IRUSR | S_IWUSR);
        if (native < 0)
            bad("cannot create private message file");
        file_ = selected;
        std::size_t offset = 0;
        int error = 0;
        while (offset < value.size()) {
            const auto count = ::write(native, value.data() + offset, value.size() - offset);
            if (count < 0 && errno == EINTR)
                continue;
            if (count <= 0) {
                error = 1;
                break;
            }
            offset += static_cast<std::size_t>(count);
        }
        if (::close(native) < 0 || error != 0)
            bad("cannot write or close private message file");
#endif
        return selected;
    }
    void finish() {
        std::error_code error;
        const bool removed = std::filesystem::remove(file_, error);
        if (error || !removed)
            bad("cannot remove private message file");
        file_.clear();
        if (!std::filesystem::remove(directory_, error) || error)
            bad("cannot remove private message directory");
        directory_.clear();
    }

  private:
    std::filesystem::path directory_;
    std::filesystem::path file_;
};

std::string body_file(const std::filesystem::path &path) {
    if (!std::filesystem::is_regular_file(path))
        bad("body_file must be a readable regular file");
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
        bad("cannot read body_file");
    const auto length = input.tellg();
    if (length < 0 || length > 65536 + 3)
        bad("body_file exceeds 64 KiB limit");
    std::string bytes(static_cast<std::size_t>(length), '\0');
    input.seekg(0);
    if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size())) ||
        input.peek() != std::char_traits<char>::eof())
        bad("cannot read stable body_file");
    if (bytes.starts_with("\xef\xbb\xbf"))
        bytes.erase(0, 3);
    return normalize_body(bytes);
}
std::string inherited(std::string_view key) {
#ifdef _WIN32
    const std::wstring name(key.begin(), key.end());
    const auto size = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
    if (size == 0)
        return {};
    std::wstring value(size, L'\0');
    const auto count = GetEnvironmentVariableW(name.c_str(), value.data(), size);
    if (count == 0 || count >= size)
        bad("cannot read authentication environment");
    value.resize(count);
    const auto bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                           static_cast<int>(count), nullptr, 0, nullptr, nullptr);
    if (bytes == 0)
        bad("cannot encode authentication environment");
    std::string out(static_cast<std::size_t>(bytes), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(count),
                             out.data(), bytes, nullptr, nullptr))
        bad("cannot encode authentication environment");
    return out;
#else
    const std::string name{key};
    const auto *value = std::getenv(name.c_str());
    return value ? std::string{value} : std::string{};
#endif
}
} // namespace

void validate_smtp(const Smtp &settings) {
    const bool numeric_host = std::ranges::all_of(
        settings.host, [](char ch) { return (ch >= '0' && ch <= '9') || ch == '.'; });
    if (!(numeric_host ? ipv4(settings.host) : (dns(settings.host) || ipv6(settings.host))))
        bad("invalid host; use a hostname, IPv4 or bracketed IPv6");
    if (settings.port < 1 || settings.port > 65535)
        bad("port must be 1..65535");
    if (settings.tls != "starttls" && settings.tls != "implicit")
        bad("tls must be starttls or implicit");
    if (!mailbox(settings.from))
        bad("invalid sender mailbox");
    if (settings.to.empty() || settings.to.size() > 32)
        bad("expected 1..32 recipient mailboxes");
    std::set<std::string> recipients;
    for (auto recipient : settings.to) {
        if (!mailbox(recipient))
            bad("invalid recipient mailbox");
        const auto at = recipient.find('@');
        for (auto i = at + 1; i < recipient.size(); ++i)
            if (recipient[i] >= 'A' && recipient[i] <= 'Z')
                recipient[i] = static_cast<char>(recipient[i] + ('a' - 'A'));
        if (!recipients.insert(recipient).second)
            bad("duplicate recipient mailbox");
    }
    if (settings.subject.empty() || settings.subject.size() > 512)
        bad("subject must contain 1..512 UTF-8 bytes");
    text(settings.subject, true);
    if (settings.client.empty() || settings.client.find('\0') != std::string::npos)
        bad("invalid SMTP client path");
    if (settings.connect_timeout_seconds < 1 || settings.connect_timeout_seconds > 3600 ||
        settings.timeout_seconds < settings.connect_timeout_seconds ||
        settings.timeout_seconds > 3600)
        bad("invalid connection/total timeout range");
    if (settings.body && settings.body_file)
        bad("body and body_file are mutually exclusive");
    if (settings.body)
        (void)normalize_body(*settings.body);
    if (settings.auth && (!valid_environment_name(settings.auth->username_env) ||
                          !valid_environment_name(settings.auth->password_env)))
        bad("invalid authentication environment variable name");
}

std::string smtp_message(const Smtp &settings, std::string_view body,
                         std::chrono::system_clock::time_point now, std::string_view id) {
    validate_smtp(settings);
    if (id.empty() || !std::ranges::all_of(id, [](char ch) { return alnum(ch) || ch == '-'; }))
        bad("invalid message identifier");
    const auto native = std::chrono::system_clock::to_time_t(now);
    std::tm parts{};
#ifdef _WIN32
    if (gmtime_s(&parts, &native) != 0)
        bad("cannot format email date");
#else
    if (!gmtime_r(&native, &parts))
        bad("cannot format email date");
#endif
    std::ostringstream date;
    date.imbue(std::locale::classic());
    date << std::put_time(&parts, "%a, %d %b %Y %H:%M:%S +0000");
    if (!date)
        bad("cannot format email date");
    std::string out = "From: " + settings.from + "\r\nTo: ";
    for (std::size_t i = 0; i < settings.to.size(); ++i) {
        if (i != 0)
            out += ",\r\n ";
        out += settings.to[i];
    }
    out += "\r\n" + subject_header(settings.subject) + "Date: " + date.str() + "\r\nMessage-ID: <" +
           std::string{id} + "@tatr.invalid>\r\n";
    out += "MIME-Version: 1.0\r\nContent-Type: text/plain; "
           "charset=UTF-8\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\n";
    out += encoded_body(normalize_body(body));
    if (out.size() > 512 * 1024)
        bad("generated message exceeds 512 KiB limit");
    return out;
}

std::string smtp_client_config(const Smtp &settings, const std::filesystem::path &message,
                               std::string_view username, std::string_view password) {
    validate_smtp(settings);
    const auto line = [](std::string_view key, std::string_view value) {
        return std::string{key} + " = " + quoted(value) + "\n";
    };
    std::string out = "silent\ngloboff\nssl-reqd\n";
    out += line("url", (settings.tls == "implicit" ? "smtps://" : "smtp://") + settings.host + ":" +
                           std::to_string(settings.port));
    out += line("proto", "=smtp,smtps");
    out += line("proxy", "") + line("noproxy", "*");
    out += line("mail-from", settings.from);
    for (const auto &recipient : settings.to)
        out += line("mail-rcpt", recipient);
    out += line("upload-file", path_text(message));
    out += line("connect-timeout", std::to_string(settings.connect_timeout_seconds));
    out += line("max-time", std::to_string(settings.timeout_seconds));
    if (settings.ca_file)
        out += line("cacert", path_text(*settings.ca_file));
    if (settings.auth) {
        if (username.empty() || password.empty() || username.size() > 4096 ||
            password.size() > 4096)
            bad("missing, empty or oversized authentication value");
        text(username, true);
        text(password, true);
        if (username.find(':') != std::string_view::npos)
            bad("authentication username must not contain colon");
        out += line("user", std::string{username} + ':' + std::string{password});
    }
    if (out.size() > 65536)
        bad("client configuration exceeds 64 KiB limit");
    return out;
}

ProcessResult send_smtp(const Smtp &settings, const std::optional<std::filesystem::path> &cwd,
                        const std::optional<EnvironmentOverrides> &environment,
                        std::string_view default_body) {
    const auto lookup = [&](const std::string &key) {
        if (environment) {
            const auto value = environment->find(environment_name_key(key));
            if (value != environment->end())
                return value->second;
        }
        return inherited(key);
    };
    const auto username = settings.auth ? lookup(settings.auth->username_env) : std::string{};
    const auto password = settings.auth ? lookup(settings.auth->password_env) : std::string{};
    const auto body = settings.body_file ? body_file(*settings.body_file)
                      : settings.body    ? *settings.body
                                         : std::string{default_body};
    const auto message =
        smtp_message(settings, body, std::chrono::system_clock::now(), random_name());
    MessageFile file;
    try {
        const auto path = file.write(message);
        const auto config = smtp_client_config(settings, path, username, password);
        const std::string args[]{settings.client, "-q", "--config", "-"};
        const auto result = run_process(args, cwd, environment, ProcessInput{config, true});
        if (result.exit_code == 0 && result.signal == 0)
            file.finish();
        else {
            // Preserve the client's first failure even if explicit cleanup also fails.
            try {
                file.finish();
            } catch (const std::exception &) {
            }
        }
        return result;
    } catch (...) {
        try {
            file.finish();
        } catch (const std::exception &) {
        }
        throw;
    }
}
} // namespace tatr
