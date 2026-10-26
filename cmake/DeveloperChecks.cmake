option(TATR_STRICT "Treat project C++ warnings as errors" OFF)
option(TATR_SANITIZERS "Instrument tests, project code, and parser with ASan/UBSan" OFF)

if(TATR_SANITIZERS AND (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
    NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_C_COMPILER_ID STREQUAL "Clang"))
    message(FATAL_ERROR "TATR_SANITIZERS currently requires Linux Clang for both C and C++")
endif()

function(tatr_developer_checks target)
    if(TATR_STRICT)
        if(MSVC)
            target_compile_options(${target} PRIVATE /WX /w14242 /w14254 /w14456 /w14457 /w14458)
        else()
            target_compile_options(${target} PRIVATE -Werror -Wconversion -Wsign-conversion -Wshadow)
        endif()
    endif()
    tatr_sanitizers(${target})
endfunction()

function(tatr_sanitizers target)
    if(TATR_SANITIZERS)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined
            -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
    endif()
endfunction()
