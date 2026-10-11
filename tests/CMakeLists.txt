# tests/CMakeLists.txt —— 测试构建
#
# 测试框架：vendored doctest（third_party/doctest/doctest.h），唯一来源。
# 每个用例文件对应一组主题；分层校验与生成物校验走 cmake 的 check_* 目标
# （它们是"全仓库"属性，不属于任何单个用例）。

add_executable(termchart_tests
    main.cpp

    # ── core ──
    unit/core/test_core.cpp
    unit/core/test_grapheme.cpp
    unit/core/test_expected.cpp
    unit/core/test_buffer.cpp

    # ── util ──
    unit/util/test_utf8.cpp
    unit/util/test_grapheme.cpp
    unit/util/test_wcwidth.cpp
    unit/util/test_text.cpp
    unit/util/test_util_misc.cpp

    # ── color ──
    unit/color/test_color.cpp
    unit/color/test_color_resolve.cpp

    # ── term ──
    unit/term/test_term.cpp

    # ── api ──
    unit/api/test_api.cpp

    # ── render ──
    unit/render/test_render.cpp
    unit/render/test_diff_damage.cpp
    unit/render/test_sgr.cpp

    # ── scene ──
    unit/scene/test_scene.cpp
    unit/scene/test_scene_bounds.cpp

    # ── canvas ──
    unit/canvas/test_canvas.cpp
    unit/canvas/test_canvas_geometry.cpp
    unit/canvas/test_canvas_path_layer.cpp

    # ── util ──（与既有 misc/grapheme/text/utf8/wcwidth 并列）
    unit/util/test_base64.cpp
    unit/util/test_geom.cpp
    unit/util/test_util_time.cpp

    # ── scale ──
    unit/scale/test_scale.cpp
    unit/scale/test_nice.cpp
    unit/scale/test_ticks.cpp
    unit/scale/test_scale_registry.cpp

    # ── image ──
    unit/image/test_field.cpp
    unit/image/test_bitmap.cpp
    unit/image/test_png_encode.cpp
    unit/image/test_image_convert.cpp

    # ── raster ──
    unit/raster/test_raster_braille.cpp
    unit/raster/test_raster_halfblock.cpp
    unit/raster/test_raster_quadrant.cpp
    unit/raster/test_raster_block.cpp
    unit/raster/test_raster_ascii.cpp
    unit/raster/test_raster_registry.cpp

    # ── layout ──
    unit/layout/test_layout_axis.cpp

    # ── chart ──
    unit/chart/test_chart_registry.cpp
    unit/chart/test_chart_cartesian.cpp

    # ── document ──
    unit/document/test_doc_ir.cpp

    # ── protocol ──
    unit/protocol/test_protocol_sixel.cpp
    unit/protocol/test_protocol_kitty.cpp
    unit/protocol/test_protocol_iterm2.cpp
    unit/protocol/test_protocol_fallback.cpp
    unit/protocol/test_protocol_detect.cpp

    # ── term ──
    unit/term/test_term_sequences.cpp

    # ── interaction ──
    unit/interaction/test_interaction_input.cpp
    unit/interaction/test_interaction_hit.cpp

    # ── graph ──
    unit/graph/test_graph.cpp
    unit/graph/test_graph_shapes.cpp
    unit/graph/test_wiregrid.cpp
    unit/graph/test_graph_layout.cpp
    unit/graph/test_graph_layout_tree.cpp
    unit/graph/test_graph_layout_force.cpp
    unit/graph/test_graph_route.cpp
    unit/graph/test_graph_render.cpp
    unit/graph/test_diagrams_flow.cpp

    # ── backend ──
    unit/backend/test_backend_snapshot.cpp
    unit/backend/test_backend_vector.cpp
    unit/backend/test_backend_structured.cpp

    # ── 跨模块不变量 ──
    unit/test_contracts.cpp

    # ── 结构不变量 ──
    unit/test_layering.cpp

    # ── golden ──
    golden/golden_runner.cpp
    golden/test_golden_ansi_props.cpp
    golden/test_golden_exports.cpp
    golden/test_golden_scene_raster.cpp

    # ── 集成 ──
    integration/test_terminal_restore.cpp
    integration/test_full_render.cpp

    # ── 可扩展性契约实验（ROADMAP 阶段 3）──
    integration/replace_backend_html_pre.cpp
    integration/replace_chart_radial.cpp
    integration/replace_raster_custom.cpp
)

target_link_libraries(termchart_tests PRIVATE termchart)

# 测试要覆盖 Trace 级日志：编译期开关默认只到 Info（见 util/logging.hpp）。
# TERMCHART_GOLDEN_DEFAULT_DIR：快照目录的编译期默认值，让用例与 cwd 无关。
target_compile_definitions(termchart_tests PRIVATE
    TERMCHART_MAX_LOG_LEVEL=5
    TERMCHART_GOLDEN_DEFAULT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../tests/golden")

target_include_directories(termchart_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/doctest
)

termchart_set_warnings(termchart_tests)
termchart_apply_sanitizers(termchart_tests)

add_test(NAME termchart_tests COMMAND termchart_tests)

# 单个用例快跑：ctest -R golden
add_test(NAME tests_golden COMMAND termchart_tests --test-case=*golden*)
add_test(NAME tests_render COMMAND termchart_tests --test-case=*render*)
add_test(NAME tests_core COMMAND termchart_tests --test-case=*core*)
add_test(NAME tests_util COMMAND termchart_tests --test-case=*util*)
