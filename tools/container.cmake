cmake_minimum_required(VERSION 3.25)
if(NOT MODE MATCHES "^(format|format-check|analyze|quality|sanitize)$")
    message(FATAL_ERROR "Choose MODE=format, format-check, analyze, quality, or sanitize")
endif()
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
find_program(podman NAMES podman REQUIRED)
execute_process(COMMAND "${podman}" build -t localhost/tatr-quality:llvm19.1.4
    -f "${root}/tools/Containerfile" "${root}/tools" COMMAND_ERROR_IS_FATAL ANY)
set(output "${root}/build/quality-linux")
if(MODE STREQUAL "sanitize")
    set(output "${root}/build/sanitizer-linux")
endif()
file(MAKE_DIRECTORY "${output}")
set(access ro)
if(MODE STREQUAL "format")
    set(access rw)
endif()
execute_process(COMMAND "${podman}" run --rm
    -v "${root}:/src:${access}" -v "${output}:/out:rw"
    localhost/tatr-quality:llvm19.1.4
    cmake "-DMODE=${MODE}" -P /src/tools/linux-checks.cmake
    COMMAND_ERROR_IS_FATAL ANY)
