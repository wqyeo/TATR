cmake_minimum_required(VERSION 3.25)
if(NOT MODE MATCHES "^(format|format-check|analyze|quality|sanitize)$")
    message(FATAL_ERROR "Unknown check mode: ${MODE}")
endif()
execute_process(COMMAND clang-format --version OUTPUT_VARIABLE format_version
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND clang-tidy --version OUTPUT_VARIABLE tidy_version
    COMMAND_ERROR_IS_FATAL ANY)
if(NOT format_version MATCHES "version 19.1.4" OR NOT tidy_version MATCHES "version 19.1.4")
    message(FATAL_ERROR "Quality checks require clang-format and clang-tidy 19.1.4")
endif()
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
file(GLOB sources "${root}/src/*.cpp" "${root}/src/*.hpp" "${root}/tests/*.cpp")
if(MODE STREQUAL "format")
    execute_process(COMMAND clang-format -i ${sources} COMMAND_ERROR_IS_FATAL ANY)
    return()
endif()
if(MODE STREQUAL "quality" OR MODE STREQUAL "format-check")
    execute_process(COMMAND clang-format --dry-run --Werror ${sources}
        COMMAND_ERROR_IS_FATAL ANY)
endif()
if(MODE STREQUAL "format-check")
    return()
endif()
set(preset quality)
if(MODE STREQUAL "sanitize")
    set(preset sanitize)
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --preset "${preset}" -B /out
    WORKING_DIRECTORY "${root}" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${CMAKE_COMMAND}" --build /out --parallel 2
    COMMAND_ERROR_IS_FATAL ANY)
if(MODE STREQUAL "analyze" OR MODE STREQUAL "quality")
    # Iterate the actual compile database: Linux must not analyze Windows-only
    # translation units, and vendored parser sources are outside our lint policy.
    file(READ /out/compile_commands.json database)
    string(JSON count LENGTH "${database}")
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON source GET "${database}" ${index} file)
        string(FIND "${source}" "${root}/src/" project_source)
        string(FIND "${source}" "${root}/tests/" test_source)
        if((project_source EQUAL 0 OR test_source EQUAL 0) AND source MATCHES "\\.cpp$")
            message(STATUS "Analyzing ${source}")
            execute_process(COMMAND clang-tidy -p /out "${source}"
                COMMAND_ERROR_IS_FATAL ANY)
        endif()
    endforeach()
endif()
if(NOT MODE STREQUAL "analyze")
    # Failures must remain fatal, including recoverable UBSan findings in tests
    # that normally expect a nonzero application exit status.
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env
        ASAN_OPTIONS=halt_on_error=1:detect_leaks=1
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
        ctest --test-dir /out --output-on-failure --parallel 2
        COMMAND_ERROR_IS_FATAL ANY)
endif()
