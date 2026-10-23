# Sanitizers.cmake —— ASan / UBSan / TSan 开关
#
# 注意平台现实：MinGW 不支持 TSan；CI 的 Windows 作业应跳过该预设。
# 用法：termchart_apply_sanitizers(<target>)

option(TERMCHART_ENABLE_ASAN "启用 AddressSanitizer + UBSan" OFF)
option(TERMCHART_ENABLE_UBSAN "启用 UndefinedBehaviorSanitizer" OFF)
option(TERMCHART_ENABLE_TSAN "启用 ThreadSanitizer" OFF)

# 全局应用（include 之后创建的所有目标：termchart / examples / tests / bench 全覆盖）。
# 根因：termchart 静态库被插桩后，任何链接它的可执行（examples/benches）都必须在
# **编译与链接两端**都带 sanitizer/coverage flag；只给 lib/tests 打 per-target flag，
# 会让 examples 链插桩静态库时缺运行时符号（__asan_* / __tsan_* / __gcov*）而链接
# 失败（CI 的 asan/tsan/coverage 作业正是死于此）。add_compile_options/add_link_options
# 在顶层 include 处设置，对随后 add_subdirectory 的 examples/tests/bench 一律生效。
set(termchart_san_flags "")
if(TERMCHART_ENABLE_ASAN)
    list(APPEND termchart_san_flags "-fsanitize=address,undefined" "-fno-omit-frame-pointer")
endif()
if(TERMCHART_ENABLE_UBSAN AND NOT TERMCHART_ENABLE_ASAN)
    list(APPEND termchart_san_flags "-fsanitize=undefined" "-fno-omit-frame-pointer")
endif()
if(TERMCHART_ENABLE_TSAN)
    list(APPEND termchart_san_flags "-fsanitize=thread")
endif()
if(termchart_san_flags)
    add_compile_options(${termchart_san_flags})
    add_link_options(${termchart_san_flags})
endif()
if(TERMCHART_ENABLE_COVERAGE AND NOT MSVC)
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

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
