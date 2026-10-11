# tests/fuzz/CMakeLists.txt —— fuzz 目标（默认不构建，见 TERMCHART_ENABLE_FUZZING）
#
# 为什么不自带 libFuzzer：本仓库的硬约定是"每个外部依赖都必须可缺失"。因此每个
# 目标都能以三种方式独立运行（见 fuzz_common.hpp）：
#   * `--self-test`   ：内置畸形语料，确定性、零文件依赖 —— 这一档进 ctest 冒烟；
#   * `--iterations N`：确定性 PRNG 生成随机输入（手工压测）；
#   * `<file>...`     ：把文件内容当输入（复现崩溃）。
# 要接 libFuzzer，只需在目标里定义 LLVMFuzzerTestOneInput 调同一个 target 函数；
# 构建时的 ASan/UBSan 由 cmake/Sanitizers.cmake 提供（TERMCHART_ENABLE_* 开关）。
if(NOT TERMCHART_ENABLE_FUZZING)
    return()
endif()

set(_termchart_fuzz_targets
    fuzz_utf8
    fuzz_grapheme
    fuzz_input_parser
    fuzz_colormap
    fuzz_sixel
    fuzz_png)

foreach(fuzz ${_termchart_fuzz_targets})
    add_executable(${fuzz} ${fuzz}.cpp)
    target_link_libraries(${fuzz} PRIVATE termchart)
    target_include_directories(${fuzz} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    termchart_set_warnings(${fuzz})
    termchart_apply_sanitizers(${fuzz})
    set_target_properties(${fuzz} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fuzz")
endforeach()

# 冒烟档：`--self-test` 跑内置语料并断言"不崩、不泄漏异常"。默认构建（fuzzing OFF）
# 时这些目标不存在，因此这里必须同时看 TERMCHART_BUILD_TESTS。
if(TERMCHART_BUILD_TESTS)
    foreach(fuzz ${_termchart_fuzz_targets})
        add_test(NAME ${fuzz}_self_test COMMAND ${fuzz} --self-test)
    endforeach()
endif()
