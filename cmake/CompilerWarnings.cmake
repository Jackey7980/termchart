# CompilerWarnings.cmake —— 警告开关统一处
#
# 不在各 CMakeLists 里手写 -Wxxx：那样会出现"某些目标没开 -Wextra 这种乌龙。
# 用法：termchart_set_warnings(<target> [WERROR])

function(termchart_set_warnings target)
    cmake_parse_arguments(ARG "WERROR" "" "" ${ARGN})

    set(gcc_like "$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>")
    set(msvc_like "$<COMPILE_LANG_AND_ID:CXX,MSVC>")

    set(warnings "")
    list(APPEND warnings
        "$<${gcc_like}:-Wall>"
        "$<${gcc_like}:-Wextra>"
        "$<${gcc_like}:-Wpedantic>"
        "$<${gcc_like}:-Wshadow>"
        "$<${gcc_like}:-Wnon-virtual-dtor>"
        "$<${gcc_like}:-Wold-style-cast>"
        "$<${gcc_like}:-Wcast-align>"
        "$<${gcc_like}:-Wunused>"
        "$<${gcc_like}:-Woverloaded-virtual>"
        "$<${gcc_like}:-Wconversion>"
        "$<${gcc_like}:-Wsign-conversion>"
        "$<${gcc_like}:-Wdouble-promotion>"
        "$<${gcc_like}:-Wformat=2>"
    )

    # 第三方/系统头不参与：只对我们自己的代码较真。
    list(APPEND warnings "$<${gcc_like}:-Wno-unused-parameter>")
    if(ARG_WERROR)
        list(APPEND warnings "$<${gcc_like}:-Werror>")
    endif()

    set(msvc_warnings "")
    list(APPEND msvc_warnings
        "$<${msvc_like}:/W4>"
        "$<${msvc_like}:/permissive->"
    )
    if(ARG_WERROR)
        list(APPEND msvc_warnings "$<${msvc_like}:/WX>")
    endif()

    target_compile_options(${target} PRIVATE ${warnings} ${msvc_warnings})
endfunction()
