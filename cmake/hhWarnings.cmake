# hh_set_warnings(<target>) enables the warning set every hh target is built with.
# The code is expected to stay free of warnings under all of them.
function(hh_set_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        # -Wsign-conversion is part of clang's -Wconversion; ask GCC for it as well.
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
        if(HH_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(HH_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    endif()
endfunction()
