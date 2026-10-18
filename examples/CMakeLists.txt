# examples/CMakeLists.txt —— 示例
#
# 每个示例都是"阶段出口条件"的可执行形式：ROADMAP 里写"example 01 跑通"，
# 跑的就是这里。示例只链接库，不加任何特殊编译开关。

foreach(example
        01_hello_color
        02_sine_wave
        05_showcase
        06_dashboard
        07_fractal
        08_fractal_levels
        03_function_plot
        03_line_chart
        04_bar_chart
        04_heatmap
        09_radar
        10_gauge
        11_wordcloud
        12_treemap
        13_sankey
        18_uml_sequence
        28_ascii_mode
        29_256color_mode
        14_flowchart
        15_state_machine
        16_mindmap
        17_uml_class
        20_force_graph
        19_metro_map
        21_hover_tooltip
        22_zoom_pan
        30_legend_toggle
        32_quick_one_liner
        33_mermaid_flowchart
        42_single_header_usage
        23_multi_chart_dashboard
        24_theme_switching
        25_animation
        26_realtime_stream
        27_background_thread
        31_plugin_custom_chart
        39_headless_render
        40_immediate_mode
        41_reactive_dashboard
        43_contour
        44_violin
        45_ridgeline
        46_parallel
        47_gantt
        48_forest
        49_ecdf
        50_qq
        51_hexbin
        52_chord
        53_er_diagram
        54_dag
        55_gui_scene_png
        56_sugiyama
        60_dense_sugiyama
        58_gui_hover
        34_export_svg
        37_export_latex
        35_export_html
        36_export_json
        38_image_sixel
        40_advanced_master
        59_quick_plot
        62_zoom_pan
        67_world_map
        68_cn_province
        69_city_plot
        70_export_pdf)
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE termchart)
    termchart_set_warnings(${example})
    set_target_properties(${example} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")
endforeach()

# 57 演示平行栈 termchart_gui：链接 termchart_gui（其再链接 termchart），
# 走 HeadlessBackend + Mount 整帧绘制 → 快照 → encode_png 导出。
add_executable(57_gui_app 57_gui_app.cpp)
target_link_libraries(57_gui_app PRIVATE termchart_gui)
termchart_set_warnings(57_gui_app)
set_target_properties(57_gui_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")

# 61 演示 TuiBackend：GUI Bitmap → 上半块真彩刷终端（零依赖、真可见）。
add_executable(61_gui_terminal 61_gui_terminal.cpp)
target_link_libraries(61_gui_terminal PRIVATE termchart_gui)
termchart_set_warnings(61_gui_terminal)
set_target_properties(61_gui_terminal PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")

# 65 演示交互闭环：合成鼠标事件驱动 Slider/Button/Checkbox → 状态联动 → 重绘 → 出帧。
add_executable(65_gui_interactive 65_gui_interactive.cpp)
target_link_libraries(65_gui_interactive PRIVATE termchart_gui)
termchart_set_warnings(65_gui_interactive)
set_target_properties(65_gui_interactive PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")

# 42 只 include 单头产物（single_include/），证明其自包含——正常 include 路径之外
# 额外挂 single_include/；注意它仍链接 termchart 库（单头管声明与内联，实现走链接）。
if(TARGET 42_single_header_usage)
    # BEFORE：single_include 必须排在链接 termchart 带来的 include/ 之前，
    # 否则 #include "termchart.hpp" 会解析到极简伞形头而不是单头产物。
    target_include_directories(42_single_header_usage BEFORE PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../single_include")
endif()

# 让 `ctest` 也能顺带确认示例可运行（不比对输出，只确认正常退出）。
# 注意：BUILD_TESTING 由 include(CTest) 定义，本仓库不用它，故看自己的开关。
if(TERMCHART_BUILD_TESTS)
    foreach(example 01_hello_color 02_sine_wave 03_function_plot 03_line_chart
                    04_bar_chart 04_heatmap 05_showcase 06_dashboard 07_fractal 08_fractal_levels
                    09_radar 10_gauge 11_wordcloud 12_treemap 13_sankey 18_uml_sequence
                    28_ascii_mode 29_256color_mode
                    14_flowchart 15_state_machine
                    16_mindmap 17_uml_class 19_metro_map 20_force_graph 21_hover_tooltip 22_zoom_pan
                    30_legend_toggle 32_quick_one_liner 33_mermaid_flowchart
                    42_single_header_usage
                    23_multi_chart_dashboard 24_theme_switching 25_animation
                    26_realtime_stream 27_background_thread 31_plugin_custom_chart
                    39_headless_render 40_immediate_mode 41_reactive_dashboard
                    43_contour 44_violin 45_ridgeline 46_parallel 47_gantt 48_forest
                    49_ecdf 50_qq 51_hexbin 52_chord 53_er_diagram 54_dag
                    55_gui_scene_png 56_sugiyama 60_dense_sugiyama 57_gui_app 61_gui_terminal
                    59_quick_plot 58_gui_hover 65_gui_interactive
                    34_export_svg 35_export_html 36_export_json 37_export_latex 38_image_sixel
                    40_advanced_master 62_zoom_pan)
        add_test(NAME example_${example} COMMAND ${example})
    endforeach()
endif()
