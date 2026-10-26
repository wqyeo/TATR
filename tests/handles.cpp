#include "process.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace {
struct Handle {
    HANDLE value = nullptr;
    ~Handle() {
        if (value)
            CloseHandle(value);
    }
    Handle() = default;
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
};
} // namespace

int wmain(int argc, wchar_t **argv) {
    if (argc != 3)
        return 90;
    try {
        const auto encoded = std::filesystem::path{argv[1]}.u8string();
        const std::string child{encoded.begin(), encoded.end()};
        const std::wstring mode{argv[2]};
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        Handle event;
        event.value = CreateEventW(&security, TRUE, FALSE, nullptr);
        if (!event.value)
            return 91;
        std::vector<std::string> command{
            child, "handle_excluded",
            std::to_string(reinterpret_cast<std::uintptr_t>(event.value))};
        if (mode == L"absent_standard_handles") {
            if (!SetStdHandle(STD_INPUT_HANDLE, nullptr) ||
                !SetStdHandle(STD_OUTPUT_HANDLE, nullptr) ||
                !SetStdHandle(STD_ERROR_HANDLE, nullptr))
                return 92;
        } else if (mode == L"launch_failure_cleanup") {
            // Preserve a recognizable PE image but make its subsystem invalid,
            // exercising CreateProcess failure after handle/attribute setup.
            const auto image =
                std::filesystem::path{argv[1]}.parent_path() / L"unsupported-subsystem.exe";
            std::filesystem::copy_file(std::filesystem::path{argv[1]}, image,
                                       std::filesystem::copy_options::overwrite_existing);
            {
                std::fstream binary(image, std::ios::binary | std::ios::in | std::ios::out);
                IMAGE_DOS_HEADER dos{};
                binary.read(reinterpret_cast<char *>(&dos), sizeof(dos));
                binary.seekp(
                    static_cast<std::streamoff>(dos.e_lfanew) +
                    static_cast<std::streamoff>(sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) +
                                                offsetof(IMAGE_OPTIONAL_HEADER, Subsystem)));
                const WORD subsystem = 0xffff;
                binary.write(reinterpret_cast<const char *>(&subsystem), sizeof(subsystem));
                if (!binary)
                    return 94;
            }
            const auto image_utf8 = image.u8string();
            const std::vector<std::string> invalid{
                std::string{image_utf8.begin(), image_utf8.end()}};
            const auto fails_at_create = [&invalid] {
                try {
                    (void)tatr::run_process(invalid);
                } catch (const std::exception &error) {
                    if (!std::string{error.what()}.starts_with(
                            "cannot launch process (Windows error "))
                        std::cerr << error.what() << '\n';
                    return std::string{error.what()}.starts_with(
                        "cannot launch process (Windows error ");
                }
                return false;
            };
            // Windows may initialize process-wide resources on the first call.
            if (!fails_at_create())
                return 95;
            DWORD before = 0;
            if (!GetProcessHandleCount(GetCurrentProcess(), &before))
                return 93;
            for (int i = 0; i < 20; ++i) {
                if (!fails_at_create())
                    return 95;
            }
            DWORD after = 0;
            if (!GetProcessHandleCount(GetCurrentProcess(), &after) || before != after)
                return 96;
        } else if (mode != L"excluded") {
            return 97;
        }
        const auto result = tatr::run_process(command);
        if (result.exit_code != 0 || result.signal != 0)
            return 98;
        // A signaled event would prove the child received our unrelated handle.
        if (WaitForSingleObject(event.value, 0) != WAIT_TIMEOUT)
            return 99;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 100;
    }
}
