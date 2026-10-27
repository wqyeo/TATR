#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <aclapi.h>
#else
#include <sys/stat.h>
#endif
namespace {
std::string env(const char *key) {
#ifdef _WIN32
    std::size_t length = 0;
    if (getenv_s(&length, nullptr, 0, key) != 0 || length == 0)
        return {};
    std::string value(length, '\0');
    if (getenv_s(&length, value.data(), value.size(), key) != 0)
        throw std::runtime_error("cannot read fixture environment");
    value.resize(length - 1);
    return value;
#else
    const auto *value = std::getenv(key);
    return value ? value : "";
#endif
}
std::filesystem::path path(const std::string &text) {
    return std::filesystem::path{std::u8string{text.begin(), text.end()}};
}
std::string decode(std::string_view value) {
    if (value.size() < 2 || value.front() != '"' || value.back() != '"')
        throw std::runtime_error("unquoted config");
    value.remove_prefix(1);
    value.remove_suffix(1);
    std::string out;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\') {
            if (++i == value.size() || (value[i] != '\\' && value[i] != '"'))
                throw std::runtime_error("invalid config escape");
        }
        out += value[i];
    }
    return out;
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
    try {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
            const auto value = std::filesystem::path{argv[i]}.u8string();
            args.emplace_back(value.begin(), value.end());
        }
        if (args != std::vector<std::string>{"-q", "--config", "-"})
            throw std::runtime_error("unsafe SMTP argv");
        if (env("TATR_FAKE_EARLY") == "1")
            return 9;
        std::multimap<std::string, std::string> values;
        std::string line;
        while (std::getline(std::cin, line)) {
            const auto at = line.find(" = ");
            if (at != std::string::npos)
                values.emplace(line.substr(0, at), decode(std::string_view{line}.substr(at + 3)));
            else if (line != "silent" && line != "globoff" && line != "ssl-reqd")
                throw std::runtime_error("unexpected client option");
        }
        const auto get = [&](const char *key) -> const std::string & {
            const auto found = values.find(key);
            if (found == values.end())
                throw std::runtime_error("missing client config field");
            return found->second;
        };
        const auto file = path(get("upload-file"));
        const auto root = path(env("TATR_FAKE_DIR"));
        std::filesystem::create_directories(root);
#ifdef _WIN32
        PSECURITY_DESCRIPTOR security = nullptr;
        PACL acl = nullptr;
        if (GetNamedSecurityInfoW(file.parent_path().c_str(), SE_FILE_OBJECT,
                                  DACL_SECURITY_INFORMATION, nullptr, nullptr, &acl, nullptr,
                                  &security) != ERROR_SUCCESS)
            throw std::runtime_error("cannot inspect private ACL");
        const bool private_acl = acl && acl->AceCount == 1;
        LocalFree(security);
        if (!private_acl)
            throw std::runtime_error("message ACL is not private");
#else
        struct stat info{};
        if (stat(file.parent_path().c_str(), &info) != 0 || (info.st_mode & 0777) != 0700)
            throw std::runtime_error("directory permissions");
        if (stat(file.c_str(), &info) != 0 || (info.st_mode & 0777) != 0600)
            throw std::runtime_error("message permissions");
#endif
        if (values.contains("user") &&
            get("user") != env("SMTP_USERNAME") + ':' + env("SMTP_PASSWORD"))
            throw std::runtime_error("authentication precedence");
        if (!values.contains("proto") || get("proto") != "=smtp,smtps" || get("proxy") != "")
            throw std::runtime_error("protocol/proxy restrictions");
        std::filesystem::copy_file(file, root / "message.eml",
                                   std::filesystem::copy_options::overwrite_existing);
        std::ofstream summary(root / "client.txt");
        for (const auto &[key, value] : values)
            if (key != "user")
                summary << key << '=' << value << '\n';
        summary << "auth=" << (values.contains("user") ? "yes" : "no") << '\n';
        summary.close();
        // TATR must suppress both streams, not attempt to redact arbitrary server text.
        std::cout << "RAW-SMTP-SECRET " << env("SMTP_PASSWORD") << '\n';
        std::cerr << "RAW-SMTP-SECRET " << env("SMTP_PASSWORD") << '\n';
        if (env("TATR_FAKE_REMOVE") == "1")
            std::filesystem::remove(file);
        const auto result = env("TATR_FAKE_EXIT");
        return result.empty() ? 0 : std::stoi(result);
    } catch (const std::exception &error) {
        std::ofstream(path(env("TATR_FAKE_DIR")) / "failure.txt") << error.what();
        std::cerr << error.what() << '\n';
        return 88;
    }
}
