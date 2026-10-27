#pragma once

#include "environment.hpp"
#include "process.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tatr {
struct SmtpAuth {
    std::string username_env;
    std::string password_env;
};
struct Smtp {
    std::string host;
    unsigned port = 0;
    std::string tls;
    std::string from;
    std::vector<std::string> to;
    std::string subject;
    std::optional<std::string> body;
    std::optional<std::filesystem::path> body_file;
    std::optional<std::filesystem::path> ca_file;
    std::string client = "curl";
    unsigned connect_timeout_seconds = 10;
    unsigned timeout_seconds = 30;
    std::optional<SmtpAuth> auth;
};

// Validation and rendering do not access files, environment, clients or network.
void validate_smtp(const Smtp &settings);
std::string smtp_message(const Smtp &settings, std::string_view body,
                         std::chrono::system_clock::time_point now, std::string_view id);
std::string smtp_client_config(const Smtp &settings, const std::filesystem::path &message,
                               std::string_view username, std::string_view password);
ProcessResult send_smtp(const Smtp &settings, const std::optional<std::filesystem::path> &cwd,
                        const std::optional<EnvironmentOverrides> &environment,
                        std::string_view default_body);
} // namespace tatr
