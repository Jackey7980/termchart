# showcase/CMakeLists.txt —— TermChart 能力全景综合示例（仓库根新建目录）
#
# 一个"海报"把库的能力跑一遍（12 面板：chart/graph/3D/4D/math/data/io/image/
# backend），数据复杂（多尺度 fBm + 相关 + 混沌 + 4D），TUI 与 GUI 双版本。
#
#   panorama_tui  全平台，链接 termchart；默认进 ctest（非 tty 渲染一帧，确定性）。
#   panorama_gui  仅 Windows + gui_win32 插件启用时构建（真窗口 + --png 无头）。

add_executable(panorama_tui panorama_tui.cpp)
target_link_libraries(panorama_tui PRIVATE termchart)
termchart_set_warnings(panorama_tui)
if(MINGW)
    target_link_options(panorama_tui PRIVATE -static)  # 全静态：单独拷贝即可运行
endif()
set_target_properties(panorama_tui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/showcase")

# 全功能 catalog：把库的全部图表/图/后端/协议/交互/图像等逐一跑一遍（分三区登记）。
add_executable(catalog_tui
    catalog_main.cpp
    catalog_charts.cpp
    catalog_graphs.cpp
    catalog_systems.cpp)
target_link_libraries(catalog_tui PRIVATE termchart)
termchart_set_warnings(catalog_tui)
if(MINGW)
    target_link_options(catalog_tui PRIVATE -static)  # 全静态：单独拷贝即可运行
endif()
set_target_properties(catalog_tui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/showcase")

# GUI 真窗口版：走 gui_win32 平行栈（Win32Backend + SceneRenderer + Mount），
# 与 plugins/gui_win32/example 同构；--png 无头出图，CI 可验证（无需桌面）。
if(WIN32 AND TERMCHART_BUILD_GUI_WIN32 AND TARGET termchart_gui_win32)
    add_executable(panorama_gui panorama_gui.cpp)
    target_compile_definitions(panorama_gui PRIVATE TERMCHART_BUILD_GUI_WIN32)
    target_link_libraries(panorama_gui PRIVATE termchart_gui_win32)
    if(MINGW)
        target_link_options(panorama_gui PRIVATE -static)  # 全静态：单独拷贝即可运行
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(panorama_gui PRIVATE -Wall -Wextra -Wno-unused-parameter)
    endif()
    set_target_properties(panorama_gui PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/showcase")
endif()

# 冒烟测试：TUI 非 tty 一帧（stdout 被重定向 → 走一次性海报路径，退出 0）。
if(TERMCHART_BUILD_TESTS)
    add_test(NAME showcase_panorama_tui COMMAND panorama_tui --cols 120 --rows 44)
    set_tests_properties(showcase_panorama_tui PROPERTIES
        PASS_REGULAR_EXPRESSION "TermChart")
    add_test(NAME showcase_catalog_list COMMAND catalog_tui --list)
    set_tests_properties(showcase_catalog_list PROPERTIES
        PASS_REGULAR_EXPRESSION "total")
    if(WIN32 AND TERMCHART_BUILD_GUI_WIN32 AND TARGET termchart_gui_win32)
        add_test(NAME showcase_panorama_gui_png
                 COMMAND panorama_gui --png "${CMAKE_CURRENT_BINARY_DIR}/panorama_gui.png")
    endif()
endif()
