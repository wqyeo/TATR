cmake_minimum_required(VERSION 3.25)
project(tatr VERSION 0.1.0 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build configuration" FORCE)
endif()

include(GNUInstallDirs)
include(CTest)
include(cmake/DeveloperChecks.cmake)

# Vendored, pinned source: configuration and builds need no network access.
add_library(tomlc17 STATIC third_party/tomlc17/tomlc17.c)
target_include_directories(tomlc17 SYSTEM PUBLIC third_party/tomlc17)
set_target_properties(tomlc17 PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
tatr_sanitizers(tomlc17)

add_library(tatr_core STATIC src/config.cpp src/cli.cpp src/planner.cpp src/process.cpp src/runner.cpp)
target_include_directories(tatr_core PUBLIC src)
target_link_libraries(tatr_core PRIVATE tomlc17)
target_compile_features(tatr_core PUBLIC cxx_std_23)
set_target_properties(tatr_core PROPERTIES CXX_EXTENSIONS OFF)
target_compile_definitions(tatr_core PRIVATE TATR_VERSION="${PROJECT_VERSION}")

add_executable(tatr src/main.cpp)
target_link_libraries(tatr PRIVATE tatr_core)
set_target_properties(tatr PROPERTIES CXX_EXTENSIONS OFF)
if(MINGW)
    target_link_options(tatr PRIVATE -municode)
endif()

if(MSVC)
    # Avoid requiring a separate VC runtime installation.
    set_property(TARGET tomlc17 tatr_core tatr PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(tomlc17 PRIVATE /utf-8)
    target_compile_options(tatr_core PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(tatr PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(tatr_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(tatr PRIVATE -Wall -Wextra -Wpedantic)
endif()

tatr_developer_checks(tatr_core)
tatr_developer_checks(tatr)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(TARGETS tatr RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES third_party/tomlc17/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/tatr/licenses RENAME tomlc17-LICENSE)
