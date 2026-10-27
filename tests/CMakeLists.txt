set(cases
    help version no_arguments list_default list_file file_after_command
    empty_tasks absent_description empty_description unicode unicode_file dotted_name
    missing_file malformed duplicate_key missing_version wrong_version version_string
    version_float version_boolean missing_tasks tasks_array task_scalar task_array
    description_type unknown_root unknown_task
    empty_name control_name control_description
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

add_executable(tatr_test_logging logging.cpp)
target_link_libraries(tatr_test_logging PRIVATE tatr_core)
set_target_properties(tatr_test_logging PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    set_property(TARGET tatr_test_logging PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(tatr_test_logging PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(tatr_test_logging PRIVATE -Wall -Wextra -Wpedantic)
endif()
tatr_developer_checks(tatr_test_logging)
if(MINGW)
    target_link_options(tatr_test_logging PRIVATE -municode)
endif()
add_test(NAME logging.streams_and_rendering COMMAND tatr_test_logging
    $<TARGET_FILE:tatr_test_child> "${CMAKE_CURRENT_BINARY_DIR}/logging/streams")
set(logging_cases default file append file_only unicode tokens defaults absolute cwd child_output child_io escaped_task off off_failure warn error
    failure filtered_failure file_only_failure launch dotenv group shared open_failure list plan unknown_target group_args
    invalid_level level_type console_type silent logging_type file_type unknown_field
    unknown_file_field directory_type filename_type empty_directory nul_directory bad_filename
    bad_token reserved)
if(WIN32)
    list(APPEND logging_cases drive_relative large_status)
else()
    list(APPEND logging_cases signal file_write_failure)
endif()
foreach(case IN LISTS logging_cases)
    add_test(NAME logging.${case} COMMAND ${CMAKE_COMMAND}
        "-DTATR=$<TARGET_FILE:tatr>" "-DCHILD=$<TARGET_FILE:tatr_test_child>"
        "-DCASE=${case}" "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/logging/${case}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/logging.cmake)
endforeach()

foreach(case IN LISTS cases)
    add_test(NAME cli.${case} COMMAND ${CMAKE_COMMAND}
        "-DTATR=$<TARGET_FILE:tatr>"
        "-DCASE=${case}"
        "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cases/${case}"
        "-DVERSION=${PROJECT_VERSION}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cli.cmake)
endforeach()

set(dotenv_cases
    basic inherited inline_precedence inline_empty empty_file empty_value whitespace equals_value
    comment full_comments hash_literal hash_quoted single_quotes double_quotes escapes literal
    unquoted_quotes export_prefix export_name crlf bom no_final_newline
    duplicate case_variants malformed unknown_escape unclosed_quote trailing_text multiline
    name_digit name_dash name_unicode name_empty nul_value invalid_utf8 invalid_continuation
    overlong_utf8 surrogate_utf8 high_utf8 truncated_utf8 lone_cr bare_trailing_cr error_line
    missing not_file generated_file absolute_file unicode_file relative_config cwd_independent
    isolation no_discovery list_missing plan_missing list_malformed unrelated_missing
    type_number type_array empty_path nul_path grouping invalid_unrelated
    path_file path_inline path_empty size_limit size_over_limit inline_does_not_hide_error
)
if(WIN32)
    list(APPEND dotenv_cases windows_layer_casing drive_relative)
endif()
foreach(case IN LISTS dotenv_cases)
    add_test(NAME dotenv.${case} COMMAND ${CMAKE_COMMAND}
        "-DTATR=$<TARGET_FILE:tatr>" "-DCHILD=$<TARGET_FILE:tatr_test_child>"
        "-DCASE=${case}" "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/dotenv/${case}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/dotenv.cmake)
endforeach()

set(argument_cases
    append_order no_separator empty_separator empty_values encoding literal flags_after_separator
    file_before file_middle file_after_target prerequisite_isolation grouping_reject
    grouping_empty_separator grouping_empty_argument separator_no_target separator_before_command
    list_separator plan_separator empty_target file_value_separator extra_before_separator
    extra_without_separator unknown_target cwd env child_failure prerequisite_failure launch_failure
    invalid_unrelated
)
if(WIN32)
    list(APPEND argument_cases combined_command_limit)
endif()
foreach(case IN LISTS argument_cases)
    add_test(NAME arguments.${case} COMMAND ${CMAKE_COMMAND}
        "-DTATR=$<TARGET_FILE:tatr>" "-DCHILD=$<TARGET_FILE:tatr_test_child>"
        "-DCASE=${case}" "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/arguments/${case}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/arguments.cmake)
endforeach()

set(env_cases
    inherit replace empty unicode literal new_name empty_table isolation inherited_unusual_name
    path_absolute path_relative path_empty path_empty_entry path_missing path_without_cwd
    path_unicode inherited_path_unset lookup_preserved
    list_readonly plan_readonly invalid_unrelated grouping
    type_string type_array type_number value_number value_boolean value_array value_table
    name_empty name_digit name_equals name_dash name_unicode name_control name_nul nul_value
    case_variants launch_failure child_failure size_failure no_shell_fallback
)
if(WIN32)
    list(APPEND env_cases windows_casing windows_path_casing)
else()
    list(APPEND env_cases path_denied path_continue)
endif()
foreach(case IN LISTS env_cases)
    add_test(NAME env.${case} COMMAND ${CMAKE_COMMAND}
        "-DTATR=$<TARGET_FILE:tatr>" "-DCHILD=$<TARGET_FILE:tatr_test_child>"
        "-DCASE=${case}" "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/env/${case}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/env.cmake)
endforeach()

set(cwd_cases
    relative absolute dot unicode relative_file independent parent_unchanged
    missing not_directory prerequisite_created relative_executable absolute_executable
    path_absolute path_relative path_empty_entry path_missing
    list_missing plan_missing unrelated_missing invalid_unrelated
    empty type_integer type_array type_boolean nul_path grouping launch_failure child_failure
)
if(WIN32)
    list(APPEND cwd_cases drive_relative drive_relative_executable drive_relative_path)
else()
    list(APPEND cwd_cases symlink_parent)
endif()
foreach(case IN LISTS cwd_cases)
    add_test(NAME cwd.${case} COMMAND ${CMAKE_COMMAND}
        "-DTATR=$<TARGET_FILE:tatr>" "-DCHILD=$<TARGET_FILE:tatr_test_child>"
        "-DCASE=${case}" "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cwd/${case}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cwd.cmake)
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
