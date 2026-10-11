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
        14_flowchart
        15_state_machine
        16_mindmap
        17_uml_class
        20_force_graph
        19_metro_map
        21_hover_tooltip
        22_zoom_pan
        32_quick_one_liner
        33_mermaid_flowchart
        34_export_svg
        35_export_html
        36_export_json
        38_image_sixel
        40_advanced_master)
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE termchart)
    termchart_set_warnings(${example})
    set_target_properties(${example} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")
endforeach()

# 让 `ctest` 也能顺带确认示例可运行（不比对输出，只确认正常退出）。
# 注意：BUILD_TESTING 由 include(CTest) 定义，本仓库不用它，故看自己的开关。
if(TERMCHART_BUILD_TESTS)
    foreach(example 01_hello_color 02_sine_wave 03_function_plot 03_line_chart
                    04_bar_chart 04_heatmap 05_showcase 06_dashboard 07_fractal 08_fractal_levels 14_flowchart 15_state_machine
                    16_mindmap 17_uml_class 19_metro_map 20_force_graph 21_hover_tooltip 22_zoom_pan
                    32_quick_one_liner 33_mermaid_flowchart
                    34_export_svg 35_export_html 36_export_json 38_image_sixel
                    40_advanced_master)
        add_test(NAME example_${example} COMMAND ${example})
    endforeach()
endif()
