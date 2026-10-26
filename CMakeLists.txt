cmake_minimum_required(VERSION 3.25)
project(tatr VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build configuration" FORCE)
endif()

include(GNUInstallDirs)

add_executable(tatr src/main.cpp)
target_compile_features(tatr PRIVATE cxx_std_23)
set_target_properties(tatr PROPERTIES CXX_EXTENSIONS OFF)
target_compile_definitions(tatr PRIVATE TATR_VERSION="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(tatr PRIVATE /W4 /permissive- /utf-8)
    # Avoid requiring a separate VC runtime installation.
    set_property(TARGET tatr PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_compile_options(tatr PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS tatr RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
