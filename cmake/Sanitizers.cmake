# Sanitizers.cmake —— ASan / UBSan / TSan 开关
#
# 注意平台现实：MinGW 不支持 TSan；CI 的 Windows 作业应跳过该预设。
# 用法：termchart_apply_sanitizers(<target>)

option(TERMCHART_ENABLE_ASAN "启用 AddressSanitizer + UBSan" OFF)
option(TERMCHART_ENABLE_UBSAN "启用 UndefinedBehaviorSanitizer" OFF)
option(TERMCHART_ENABLE_TSAN "启用 ThreadSanitizer" OFF)

function(termchart_apply_sanitizers target)
    # ASan 与 TSan 互斥：同时开会直接编译失败。
    if(TERMCHART_ENABLE_TSAN AND TERMCHART_ENABLE_ASAN)
        message(FATAL_ERROR "TERMCHART_ENABLE_TSAN 与 TERMCHART_ENABLE_ASAN 不能同时开启")
    endif()

    set(flags "")
    if(TERMCHART_ENABLE_ASAN)
        list(APPEND flags "-fsanitize=address,undefined" "-fno-omit-frame-pointer")
    endif()
    if(TERMCHART_ENABLE_UBSAN AND NOT TERMCHART_ENABLE_ASAN)
        list(APPEND flags "-fsanitize=undefined" "-fno-omit-frame-pointer")
    endif()
    if(TERMCHART_ENABLE_TSAN)
        list(APPEND flags "-fsanitize=thread")
    endif()

    if(flags)
        target_compile_options(${target} PRIVATE ${flags})
        target_link_options(${target} PRIVATE ${flags})
    endif()

    if(TERMCHART_ENABLE_COVERAGE)
        if(MSVC)
            message(WARNING "MSVC 不支持 --coverage；覆盖率请用 GCC/Clang")
        else()
            target_compile_options(${target} PRIVATE --coverage -O0 -g)
            target_link_options(${target} PRIVATE --coverage)
        endif()
    endif()
endfunction()
