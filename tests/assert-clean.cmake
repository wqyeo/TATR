# Some behavioral fixtures expect exit 1 or 2. A sanitizer failure must never be
# accepted merely because its exit code and surrounding text match that fixture.
function(assert_no_sanitizer_diagnostic output error)
    if("${output}\n${error}" MATCHES "AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer|runtime error:")
        message(FATAL_ERROR "sanitizer diagnostic detected:\n${output}\n${error}")
    endif()
endfunction()
