#include "smtp.hpp"
#include "process.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> void fails(F action) {
    try {
        action();
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error("expected failure");
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
    try {
        require(argc == 3, "expected child and output directory");
        const auto encoded = std::filesystem::path{argv[1]}.u8string();
        const std::string child{encoded.begin(), encoded.end()};
        const auto root = std::filesystem::path{argv[2]};
        std::filesystem::create_directories(root);
        tatr::Smtp smtp;
        smtp.host = "localhost";
        smtp.port = 587;
        smtp.tls = "starttls";
        smtp.from = "sender@example.com";
        smtp.to = {"first@example.com", "second@example.com"};
        smtp.subject = "Deployment café 中文 😀";
        tatr::validate_smtp(smtp);
        for (const auto *host :
             {"[::1]", "[2001:db8::1]", "[::ffff:127.0.0.1]", "127.0.0.1", "smtp.example.com"}) {
            auto selected = smtp;
            selected.host = host;
            tatr::validate_smtp(selected);
        }
        for (const auto *host :
             {"", "https://host", "host/path", "user@host", "host:587", "-host", "a..b", "[1:2:3]",
              "[::1::2]", "[zz::]", "[::ffff:999.1.1.1]", "host\n"}) {
            auto selected = smtp;
            selected.host = host;
            fails([&] { tatr::validate_smtp(selected); });
        }
        for (const auto *address : {"", "a", "a@", "@b", "a\r\nBcc:x@y", "Name <x@y>", "a,b@c",
                                    "a..b@c", ".a@c", "a.@c", "a@-b", "a@b.", "é@c"}) {
            auto selected = smtp;
            selected.from = address;
            fails([&] { tatr::validate_smtp(selected); });
        }
        for (const auto *subject : {"", "line\n", "line\r", "tab\t", "\xc2\x80", "\xff"}) {
            auto selected = smtp;
            selected.subject = subject;
            fails([&] { tatr::validate_smtp(selected); });
        }
        auto selected = smtp;
        selected.subject = std::string(513, 'x');
        fails([&] { tatr::validate_smtp(selected); });
        selected = smtp;
        selected.to = {"a@Example.com", "a@example.com"};
        fails([&] { tatr::validate_smtp(selected); });
        selected = smtp;
        selected.to.clear();
        fails([&] { tatr::validate_smtp(selected); });
        selected = smtp;
        selected.timeout_seconds = 1;
        fails([&] { tatr::validate_smtp(selected); });
        selected = smtp;
        selected.body = "a";
        selected.body_file = "b";
        fails([&] { tatr::validate_smtp(selected); });
        selected = smtp;
        selected.body = "lone\r";
        fails([&] { tatr::validate_smtp(selected); });
        selected = smtp;
        selected.body = std::string(65537, 'x');
        fails([&] { tatr::validate_smtp(selected); });
        const std::string body =
            "Hello café 中文 😀\n.\ntrailing space \r\ntab\tand=equal\n" + std::string(180, 'x');
        const auto now = std::chrono::system_clock::time_point{std::chrono::seconds{0}};
        const auto message = tatr::smtp_message(smtp, body, now, "test-id");
        require(message.find("Date: Thu, 01 Jan 1970 00:00:00 +0000\r\n") != std::string::npos,
                "UTC MIME date");
        require(message.find("Message-ID: <test-id@tatr.invalid>") != std::string::npos,
                "message id");
        require(message.find("Content-Transfer-Encoding: quoted-printable\r\n") !=
                    std::string::npos,
                "body transfer encoding");
        require(message.find("trailing=20space=20\r\n") != std::string::npos,
                "trailing whitespace encoded");
        for (std::size_t i = 0; i < message.size(); ++i) {
            if (message[i] == '\n')
                require(i > 0 && message[i - 1] == '\r', "MIME must use CRLF");
            if (message[i] == '\r')
                require(i + 1 < message.size() && message[i + 1] == '\n', "MIME lone CR");
        }
        std::ofstream out(root / "message.eml", std::ios::binary);
        out << message;
        out.close();
        require(static_cast<bool>(out), "write MIME fixture");
        selected = smtp;
        selected.subject = std::string(500, 'x') + "😀";
        const auto folded = tatr::smtp_message(selected, "", now, "folded");
        require(folded.find("?=\r\n =?UTF-8?B?") != std::string::npos, "long subject folded");
        smtp.auth = tatr::SmtpAuth{"SMTP_USER", "SMTP_PASS"};
        const auto config =
            tatr::smtp_client_config(smtp, root / "spaced message.eml", "user\"\\", "pass:\"\\");
        require(config.find("user = \"user\\\"\\\\:pass:\\\"\\\\\"\n") != std::string::npos,
                "client config quoting");
        require(config.find("ssl-reqd\n") != std::string::npos &&
                    config.find("max-time = \"30\"") != std::string::npos,
                "TLS/timeouts configured");
        fails([&] { (void)tatr::smtp_client_config(smtp, root / "x", "", "secret"); });
        fails([&] { (void)tatr::smtp_client_config(smtp, root / "x", "user:bad", "secret"); });
        fails([&] { (void)tatr::smtp_client_config(smtp, root / "x", "user", "secret\ntrace"); });
        fails([&] {
            (void)tatr::smtp_client_config(smtp, root / "x", "user", std::string(4097, 'x'));
        });
        // More than pipe capacity, forcing repeated writes and draining through EOF.
        const std::string payload(1024 * 1024, 'x');
        const auto native = (root / "input.bin").u8string();
        const std::string destination{native.begin(), native.end()};
        const std::string copy[]{child, "stdin_copy", destination};
        require(tatr::run_process(copy, {}, {}, tatr::ProcessInput{payload, true}).exit_code == 0,
                "stdin copy child failed");
        std::ifstream input(root / "input.bin", std::ios::binary);
        const std::string actual{std::istreambuf_iterator<char>{input},
                                 std::istreambuf_iterator<char>{}};
        require(actual == payload, "stdin pipe lost bytes");
        const std::string early[]{child, "exit", "9"};
        require(tatr::run_process(early, {}, {}, tatr::ProcessInput{payload, true}).exit_code == 9,
                "early failure lost child status");
        const std::string closed[]{child, "exit", "0"};
        fails([&] { (void)tatr::run_process(closed, {}, {}, tatr::ProcessInput{payload, true}); });
        const std::string absent[]{"tatr-native-client-does-not-exist"};
        fails([&] { (void)tatr::run_process(absent, {}, {}, tatr::ProcessInput{payload, true}); });
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
