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

    # ── math ──
    unit/math/test_math.cpp

    # ── io ──
    unit/io/test_io.cpp

    # ── data ──
    unit/data/test_data.cpp
    unit/data/test_scalar_parse.cpp
    unit/data/test_dataframe.cpp
    unit/data/test_autoviz_pipeline.cpp

    # ── autoviz ──
    unit/autoviz/test_autoviz.cpp

    # ── util ──
    unit/util/test_utf8.cpp
    unit/util/test_grapheme.cpp
    unit/util/test_wcwidth.cpp
    unit/util/test_text.cpp
    unit/util/test_format.cpp
    unit/util/test_util_misc.cpp
    unit/util/test_geo.cpp

    # ── color ──
    unit/color/test_color.cpp
    unit/color/test_color_resolve.cpp

    # ── term ──
    unit/term/test_term.cpp
    unit/term/test_term_size_waker.cpp

    # ── api ──
    unit/api/test_api.cpp

    # ── render ──
    unit/render/test_render.cpp
    unit/render/test_diff_damage.cpp
    unit/render/test_sgr.cpp
    unit/render/test_render_compositor.cpp

    # ── scene ──
    unit/scene/test_scene.cpp
    unit/scene/test_scene_bounds.cpp
    unit/scene/test_stateful_visitor.cpp

    # ── gui ──（平行栈渲染核心）
    unit/gui/test_gui_font.cpp
    unit/gui/test_gui_cells.cpp
    unit/gui/test_gui_scene_renderer.cpp
    unit/gui/test_gui_stack.cpp
    unit/gui/test_gui_metrics.cpp
    unit/gui/test_gui_hover.cpp
    unit/gui/test_gui_theme_view.cpp

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
    unit/scale/test_scale_discrete.cpp

    # ── image ──
    unit/image/test_field.cpp
    unit/image/test_bitmap.cpp
    unit/image/test_png_encode.cpp
    unit/image/test_jpeg_encode.cpp
    unit/image/test_image_export.cpp
    unit/image/test_mp4_encode.cpp
    unit/image/test_gif_encode.cpp
    unit/image/test_webp_encode.cpp
    unit/image/test_webp_decode.cpp
    unit/image/test_png_decode.cpp
    unit/image/test_image_convert.cpp
    unit/image/test_ttf_font.cpp
    unit/image/test_ttf_font_outline.cpp
    unit/image/test_bmp.cpp
    unit/image/test_stb_image.cpp
    unit/image/test_cabac.cpp
    unit/image/test_intra_prediction.cpp
    unit/image/test_hevc_mpm.cpp
    unit/image/test_hevc_scan.cpp
    unit/image/test_hevc_interp.cpp
    unit/image/test_hevc_multiref.cpp
    unit/image/test_hevc_bslice.cpp
    unit/image/test_hevc_deblocking.cpp
    unit/image/test_hevc_sao.cpp
    unit/image/test_hevc_rdo.cpp
    unit/image/test_hevc_qp.cpp
    unit/image/test_hevc_transform.cpp
    unit/image/test_hevc_transform_skip.cpp
    unit/image/test_hevc_tu_sizes.cpp
    unit/math3d/test_math3d.cpp
    unit/math4d/test_math4d.cpp
    unit/chart3d/test_chart3d.cpp
    unit/chart4d/test_chart4d.cpp

    # ── raster ──
    unit/raster/test_raster_braille.cpp
    unit/raster/test_raster_halfblock.cpp
    unit/raster/test_raster_quadrant.cpp
    unit/raster/test_raster_block.cpp
    unit/raster/test_raster_ascii.cpp
    unit/raster/test_raster_registry.cpp
    unit/raster/test_raster_sextant.cpp

    # ── layout ──
    unit/layout/test_layout_axis.cpp
    unit/layout/test_layout_extras.cpp
    unit/layout/test_reference.cpp

    # ── chart ──
    unit/chart/test_chart_registry.cpp
    unit/chart/test_chart_cartesian.cpp
    unit/chart/test_chart_extras.cpp
    unit/chart/test_chart_area_step.cpp
    unit/chart/test_chart_hist_heatmap.cpp
    unit/chart/test_chart_cartesian_a.cpp
    unit/chart/test_chart_cartesian_b.cpp
    unit/chart/test_chart_polar.cpp
    unit/chart/test_chart_statistical.cpp
    unit/chart/test_chart_financial.cpp
    unit/chart/test_chart_hierarchical.cpp
    unit/chart/geo/test_geo_map.cpp
    unit/chart/test_chart_flow.cpp
    unit/chart/test_chart_geo.cpp
    unit/chart/test_chart_text.cpp
    unit/chart/test_chart_indicator.cpp
    unit/chart/test_chart_financial_indicators.cpp
    unit/chart/test_chart_animation.cpp
    unit/chart/test_chart_scene.cpp
    unit/chart/test_chart_to_scene_batch.cpp
    unit/chart/test_chart_to_scene_batch2.cpp
    unit/chart/test_chart_to_scene_batch3.cpp
    unit/chart/test_chart_to_scene_batch4.cpp
    unit/chart/test_chart_to_scene_batch5.cpp
    unit/chart/test_scene_animation.cpp
    unit/chart/test_chart_palette.cpp

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
    unit/interaction/test_interaction_legend.cpp
    unit/interaction/test_interaction_highlight.cpp
    unit/interaction/test_interaction_select.cpp
    unit/interaction/test_interaction_brush.cpp
    unit/interaction/test_interaction_immediate.cpp
    unit/interaction/test_interaction_viewport.cpp
    unit/interaction/test_interaction_overlay.cpp
    unit/interaction/test_interaction_data_zoom.cpp

    # ── graph ──
    unit/graph/test_graph.cpp
    unit/graph/test_graph_shapes.cpp
    unit/graph/test_graph_layout_polar.cpp
    unit/graph/test_wiregrid.cpp
    unit/graph/test_graph_layout.cpp
    unit/graph/test_graph_layout_tree.cpp
    unit/graph/test_graph_layout_force.cpp
    unit/graph/test_graph_route.cpp
    unit/graph/test_graph_route_curves.cpp
    unit/graph/test_graph_junction.cpp
    unit/graph/test_graph_render.cpp
    unit/graph/test_diagrams_flow.cpp
    unit/graph/test_diagrams_orgchart.cpp
    unit/graph/test_graph_layout_metro_cluster.cpp
    unit/graph/test_graph_route_obstacle_bundling.cpp
    unit/graph/test_graph_route_obstacle_fix.cpp
    unit/graph/test_graph_kamada_kawai.cpp
    unit/graph/test_graph_sugiyama.cpp
    unit/graph/test_diagrams_dag.cpp
    unit/graph/test_diagrams_uml.cpp
    unit/graph/test_diagrams_network.cpp
    unit/graph/test_diagrams_relation.cpp

    # ── backend ──
    unit/backend/test_backend_snapshot.cpp
    unit/backend/test_backend_vector.cpp
    unit/backend/test_backend_structured.cpp
    unit/backend/test_backend_extra.cpp
    unit/backend/test_pdf_backend.cpp
    unit/backend/test_eps_backend.cpp
    unit/backend/test_backend_text_outline.cpp

    # ── dsl ──
    unit/dsl/test_dsl_mermaid.cpp
    unit/dsl/test_dsl_dot.cpp
    unit/dsl/test_dsl_plantuml.cpp
    unit/dsl/test_dsl_csv.cpp
    unit/dsl/test_dsl_json.cpp
    unit/dsl/test_dsl_fixtures.cpp
    unit/dsl/test_dsl_d2.cpp
    unit/dsl/test_dsl_ast.cpp

    # ── network ──
    unit/network/test_network.cpp
    unit/network/test_http.cpp
    unit/network/test_proxy.cpp

    # ── 跨模块不变量 ──
    unit/test_contracts.cpp

    # ── widget ──
    unit/widget/test_widget.cpp
    unit/widget/test_controls.cpp

    # ── plugin / app ──
    unit/plugin/test_plugin.cpp
    unit/app/test_app.cpp

    # ── 结构不变量 ──
    unit/test_layering.cpp
    unit/test_fixtures.cpp

    # ── golden ──
    golden/golden_runner.cpp
    golden/test_golden_ansi_props.cpp
    golden/test_golden_exports.cpp
    golden/test_golden_scene_raster.cpp

    # ── 集成 ──
    integration/test_terminal_restore.cpp
    integration/test_full_render.cpp
    integration/test_roundtrip_scene.cpp
    integration/test_headless_pipeline.cpp
    integration/test_interaction_graph.cpp
    integration/test_app_loop.cpp
    integration/test_thread_post.cpp
    integration/test_resize.cpp

    # ── 可扩展性契约实验（ROADMAP 阶段 3）──
    integration/replace_backend_html_pre.cpp
    integration/replace_chart_radial.cpp
    integration/replace_raster_custom.cpp
)

target_link_libraries(termchart_tests PRIVATE termchart termchart_gui)

# 测试要覆盖 Trace 级日志：编译期开关默认只到 Info（见 util/logging.hpp）。
# TERMCHART_GOLDEN_DEFAULT_DIR：快照目录的编译期默认值，让用例与 cwd 无关。
target_compile_definitions(termchart_tests PRIVATE
    TERMCHART_MAX_LOG_LEVEL=5
    TERMCHART_GOLDEN_DEFAULT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../tests/golden"
    # 同理：夹具目录用编译期常量，解析样例不依赖 cwd。
    TERMCHART_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

target_include_directories(termchart_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# vendored 第三方头按 SYSTEM 引入：它的告警不该混进我们的构建输出（否则"零警告"
# 每升一次编译器就要多背几条噪声，而且我们既不该也不方便去改它）。
target_include_directories(termchart_tests SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/doctest
)

# stb 同样按 SYSTEM 引入；仅当启用 stb_image 才需要（测试直接引用 stb_image_write）。
if(TERMCHART_HAVE_STB_IMAGE)
    target_include_directories(termchart_tests SYSTEM PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/stb)
    target_compile_definitions(termchart_tests PRIVATE TERMCHART_HAVE_STB_IMAGE=1)
endif()

termchart_set_warnings(termchart_tests)
termchart_apply_sanitizers(termchart_tests)

add_test(NAME termchart_tests COMMAND termchart_tests)

# 单个用例快跑：ctest -R golden
add_test(NAME tests_golden COMMAND termchart_tests --test-case=*golden*)
add_test(NAME tests_render COMMAND termchart_tests --test-case=*render*)
add_test(NAME tests_core COMMAND termchart_tests --test-case=*core*)
add_test(NAME tests_util COMMAND termchart_tests --test-case=*util*)

# fuzz 目标：默认不构建（TERMCHART_ENABLE_FUZZING=OFF）。放在 tests/ 之下是因为
# 规格把 fuzz/ 归在 tests/ 里；开关判断与目标清单在 tests/fuzz/CMakeLists.txt 里。
if(TERMCHART_ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()
