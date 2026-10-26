cmake_minimum_required(VERSION 3.25)
if(INNER)
    execute_process(COMMAND "${PROBE}" "${MODE}"
        RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE error)
    include("${CMAKE_CURRENT_LIST_DIR}/assert-clean.cmake")
    # Match the nonzero code a behavioral test might legitimately expect.
    if("${code}" STREQUAL "0")
        message(FATAL_ERROR "Probe unexpectedly succeeded")
    endif()
    assert_no_sanitizer_diagnostic("${output}" "${error}")
    return()
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -DINNER=ON "-DPROBE=${PROBE}" "-DMODE=${MODE}"
    -P "${CMAKE_CURRENT_LIST_FILE}" RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE error)
if("${code}" STREQUAL "0" OR NOT error MATCHES "sanitizer diagnostic detected")
    message(FATAL_ERROR "${MODE}: sanitizer finding did not fail the test harness: ${output} ${error}")
endif()
