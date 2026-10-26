set(cases
    help version no_arguments list_default list_file file_after_command
    empty_tasks absent_description empty_description unicode unicode_file dotted_name
    missing_file malformed duplicate_key missing_version wrong_version version_string
    version_float version_boolean missing_tasks tasks_array task_scalar task_array
    description_type unknown_root unknown_task
    unsupported_cwd unsupported_env empty_name control_name control_description
    c1_name c1_description nul_description nul_key no_parent_search late_invalid_task
    unknown_option unknown_command extra_argument duplicate_command missing_file_argument
    empty_file_argument option_as_file duplicate_file missing_command misplaced_help
    misplaced_version
    plan_single plan_chain plan_shared plan_branches plan_unicode plan_dotted
    plan_file_before plan_file_middle plan_file_after plan_unknown plan_missing_target
    plan_extra_target plan_empty_target plan_option_target plan_named_command
    plan_long_chain plan_unrelated_missing plan_unrelated_cycle plan_empty_tasks
    depends_empty depends_type depends_item_type depends_mixed depends_empty_name
    depends_control depends_c1 depends_nul depends_duplicate depends_missing
    depends_self_cycle depends_cycle depends_disconnected_cycle
)

foreach(case IN LISTS cases)
    add_test(NAME cli.${case} COMMAND ${CMAKE_COMMAND}
        "-DTATR=$<TARGET_FILE:tatr>"
        "-DCASE=${case}"
        "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cases/${case}"
        "-DVERSION=${PROJECT_VERSION}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cli.cmake)
endforeach()

add_executable(tatr_test_child child.cpp)
target_compile_features(tatr_test_child PRIVATE cxx_std_23)
set_target_properties(tatr_test_child PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_definitions(tatr_test_child PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_property(TARGET tatr_test_child PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(tatr_test_child PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(tatr_test_child PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(MINGW)
    target_link_options(tatr_test_child PRIVATE -municode)
endif()
tatr_developer_checks(tatr_test_child)

if(TATR_SANITIZERS)
    add_executable(tatr_sanitizer_probe sanitizer-probe.cpp)
    target_compile_features(tatr_sanitizer_probe PRIVATE cxx_std_23)
    tatr_developer_checks(tatr_sanitizer_probe)
    foreach(mode address undefined)
        add_test(NAME safety.sanitizer_propagates_${mode} COMMAND ${CMAKE_COMMAND}
            "-DPROBE=$<TARGET_FILE:tatr_sanitizer_probe>" "-DMODE=${mode}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/sanitizer-propagation.cmake)
    endforeach()
endif()

foreach(case config_limit config_over_limit)
    add_test(NAME safety.${case} COMMAND ${CMAKE_COMMAND}
        "-DTATR=$<TARGET_FILE:tatr>" "-DCHILD=$<TARGET_FILE:tatr_test_child>"
        "-DCASE=${case}" "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/safety/${case}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/safety.cmake)
endforeach()

if(WIN32)
    add_executable(tatr_test_handles handles.cpp)
    target_link_libraries(tatr_test_handles PRIVATE tatr_core)
    set_property(TARGET tatr_test_handles PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(tatr_test_handles PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive-;/utf-8>)
    if(MINGW)
        target_link_options(tatr_test_handles PRIVATE -municode)
    endif()
    tatr_developer_checks(tatr_test_handles)
    foreach(case excluded absent_standard_handles launch_failure_cleanup)
        add_test(NAME safety.handles_${case}
            COMMAND tatr_test_handles $<TARGET_FILE:tatr_test_child> ${case})
    endforeach()
endif()

set(execution_cases
    arguments inherited_io inherited_cwd inherited_env path_lookup relative_executable
    unicode_executable ordering grouping empty_group stop_failure spawn_failure
    invalid_executable unknown_target missing_target extra_target empty_target
    file_selection list_readonly plan_readonly invalid_unrelated missing_dependency cycle
    exec_type exec_empty exec_item_type exec_mixed exec_empty_name exec_nul_name exec_nul_arg
    exit_two no_shell_expansion explicit_interpreter no_implicit_shell
)
if(WIN32)
    list(APPEND execution_cases windows_large_status windows_command_limit)
else()
    list(APPEND execution_cases signal_failure)
endif()
foreach(case IN LISTS execution_cases)
    add_test(NAME execution.${case} COMMAND ${CMAKE_COMMAND}
        "-DTATR=$<TARGET_FILE:tatr>"
        "-DCHILD=$<TARGET_FILE:tatr_test_child>"
        "-DCASE=${case}"
        "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/execution/${case}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/execution.cmake)
endforeach()
