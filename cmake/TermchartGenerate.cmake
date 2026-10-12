# TermchartGenerate.cmake —— 生成规则与"生成物无 diff"校验
#
# 生成物清单（禁手改，改了生成器必须重跑）：
#   include/termchart/detail/*.hpp        ← tools/gen_*.py
#   结构.md 附一 / 附四                    ← tools/gen_module_graph.py（读 TermchartModules.cmake）
#   docs/reference/module_matrix.md       ← 同上
#   single_include/termchart.hpp          ← tools/gen_single_header.py（阶段 7）
#
# check_generated 目标跑全部 --check，任何一项过期即非零退出（CI 强制）。

find_package(Python3 COMPONENTS Interpreter QUIET)

set(_gen_dir "${CMAKE_CURRENT_SOURCE_DIR}/tools")

function(_termchart_add_py_target name script)
    # 可选附加参数（cmakeARGN 除 name/script 外原样传给脚本，如 --build-dir）。
    if(NOT Python3_Interpreter_FOUND)
        return()
    endif()
    add_custom_target(${name}
        COMMAND ${Python3_EXECUTABLE} "${_gen_dir}/${script}" ${ARGN}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "运行 ${script}"
        VERBATIM)
endfunction()

_termchart_add_py_target(generate_unicode_tables gen_unicode_tables.py)
_termchart_add_py_target(generate_grapheme_tables gen_grapheme_tables.py)
_termchart_add_py_target(generate_braille_table gen_braille_table.py)
_termchart_add_py_target(generate_turbo_table gen_turbo_table.py)
_termchart_add_py_target(generate_dsl_samples gen_dsl_samples.py)
_termchart_add_py_target(generate_module_graph gen_module_graph.py)
_termchart_add_py_target(generate_api_docs gen_api_docs.py)
# 阶段 7：单头产物（拓扑排序 + 去重 + 自包含），纯头拼接，--check 可进护栏。
_termchart_add_py_target(generate_single_header gen_single_header.py)
# gallery 依赖"已构建的示例二进制"：--build-dir 用当前构建目录，
# 因此该目标只在示例构建之后才有完整产出（CI 在构建后单独跑）。
_termchart_add_py_target(generate_gallery gen_gallery.py --build-dir "${CMAKE_BINARY_DIR}")

add_custom_target(generate_all
    COMMENT "生成全部产物（数据表 / 附一 / 附四 / module_matrix）")
foreach(_t generate_unicode_tables generate_grapheme_tables generate_braille_table
        generate_turbo_table generate_dsl_samples generate_module_graph
        generate_api_docs generate_gallery generate_single_header)
    if(TARGET ${_t})
        add_dependencies(generate_all ${_t})
    endif()
endforeach()

# ── 校验：护栏三件套 ──────────────────────────────────────────────────────
if(Python3_Interpreter_FOUND)
    add_custom_target(check_layers
        COMMAND ${Python3_EXECUTABLE} "${_gen_dir}/check_layers.py"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "分层规则校验（禁止边 → 收窄边 → 层号）"
        VERBATIM)

    add_custom_target(check_header_only
        COMMAND ${Python3_EXECUTABLE} "${_gen_dir}/check_header_only.py"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "头文件自洽性校验（include/ ↔ src/ 同路径同名）"
        VERBATIM)

    add_custom_target(check_generated
        COMMAND ${Python3_EXECUTABLE} "${_gen_dir}/gen_unicode_tables.py" --check
        COMMAND ${Python3_EXECUTABLE} "${_gen_dir}/gen_grapheme_tables.py" --check
        COMMAND ${Python3_EXECUTABLE} "${_gen_dir}/gen_braille_table.py" --check
        COMMAND ${Python3_EXECUTABLE} "${_gen_dir}/gen_turbo_table.py" --check
        COMMAND ${Python3_EXECUTABLE} "${_gen_dir}/gen_dsl_samples.py" --check
        COMMAND ${Python3_EXECUTABLE} "${_gen_dir}/gen_module_graph.py" --check
        COMMAND ${Python3_EXECUTABLE} "${_gen_dir}/gen_single_header.py" --check
        # api_docs 纯头扫描，任何时刻可校验；gallery 依赖已构建示例，不进 check
        # （在 CI 的"构建后"阶段单独跑 generate_gallery，见 .github/workflows/ci.yml）。
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "生成物与生成器一致（无 diff）"
        VERBATIM)

    add_custom_target(check_headers
        COMMAND ${Python3_EXECUTABLE} "${_gen_dir}/check_headers.py"
                --compiler "${CMAKE_CXX_COMPILER}" --include "${CMAKE_CURRENT_SOURCE_DIR}/include"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "每头独立编译（自包含性）"
        VERBATIM)

    add_custom_target(check_all
        COMMENT "全部护栏：分层 + 自洽 + 生成物 + 头文件自包含")
    add_dependencies(check_all check_layers check_header_only check_generated check_headers)
endif()

# ── golden 快照 ─────────────────────────────────────────────────────────
# 快照由测试二进制自己写（TERMCHART_UPDATE_GOLDEN=1），所以这里必须先构建它。
if(Python3_Interpreter_FOUND AND TARGET termchart_tests)
    add_custom_target(generate_golden
        COMMAND ${Python3_EXECUTABLE} "${_gen_dir}/gen_golden.py"
                "$<TARGET_FILE:termchart_tests>"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "重新生成 tests/golden 快照"
        VERBATIM)
    add_dependencies(generate_golden termchart_tests)
endif()
