# plugins/gui_win32/CMakeLists.txt —— 原生 Win32 GUI 后端（可选插件）
#
# 由顶层在 TERMCHART_BUILD_GUI_WIN32=ON 且 Windows 下 add_subdirectory。静态库 +
# 一个可运行演示。仅链系统 user32/gdi32，无任何第三方依赖。
add_library(termchart_gui_win32 STATIC src/win32_backend.cpp)
target_include_directories(termchart_gui_win32 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
# PUBLIC 传 termchart_gui（header-only）→ 再传 termchart（实现 Bitmap/字体等符号）。
target_link_libraries(termchart_gui_win32 PUBLIC termchart_gui)
if(WIN32)
    target_link_libraries(termchart_gui_win32 PRIVATE user32 gdi32)
endif()

# 平台胶水层用轻量告警：Win32 的句柄/整型转换在 -Wconversion/-Wold-style-cast 下必然
# 刷屏，故不套核心的 termchart_set_warnings（按目标函数，可安全省略），只留 -Wall -Wextra。
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(termchart_gui_win32 PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

add_executable(win32_demo example/win32_demo.cpp)
target_link_libraries(win32_demo PRIVATE termchart_gui_win32)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(win32_demo PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
set_target_properties(win32_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")

# Take 2 句柄注入演示：用户自建/自销窗口，库只贴像素并接收注入事件。
# 与 win32_demo（库自建窗口的 Take 1 路径）对照，验证外部模式不破坏既有契约。
add_executable(embed_demo example/embed_demo.cpp)
target_link_libraries(embed_demo PRIVATE termchart_gui_win32)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(embed_demo PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
set_target_properties(embed_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")

# 组合图表演示：ChartWidget（柱+折线+面积）+ 悬停信息框 + 窗口缩放自适应。
add_executable(chart_demo example/chart_demo.cpp)
target_link_libraries(chart_demo PRIVATE termchart_gui_win32)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(chart_demo PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
set_target_properties(chart_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")

# 恒生指数日 K 线演示：CandlestickWidget（影线+实体+绿涨红跌+MSN 风格价格头/统计条）
# + 悬停 OHLC 信息框 + 窗口缩放自适应。数据来自东方财富（真实 OHLC）。
add_executable(candlestick_demo example/candlestick_demo.cpp)
target_link_libraries(candlestick_demo PRIVATE termchart_gui_win32)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(candlestick_demo PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
set_target_properties(candlestick_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")

# 熊猫：TermChart 的 Scene -> SceneRenderer -> TkCanvas::blit 全链路画一张插画，
# 证明 GUI 栈不止能画图表。--png 无头出图，--shot 存窗口首帧。
add_executable(panda_demo example/panda_demo.cpp)
target_link_libraries(panda_demo PRIVATE termchart_gui_win32)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(panda_demo PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
set_target_properties(panda_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")

# 城市生活气象看板：ChartWidget（万能插座）驱动 ComboRenderer —— 上半屏 24h 温度
# （平滑折线+面积+最高温高亮），下半屏 7 天降水概率柱+最高/最低温折线。数据来自
# Open-Meteo（免费、无 Key、HTTPS），后台线程 WinInet 拉取，失败回退内嵌真实样本。
add_executable(weather_demo example/weather_demo.cpp)
target_link_libraries(weather_demo PRIVATE termchart_gui_win32 wininet)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(weather_demo PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
set_target_properties(weather_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")

# 4D 超立方体：math4d + chart4d::Projector4D 两阶段投影 → chart4d::wire4d 产 Scene →
# SceneRenderer（超采样抗锯齿真彩）→ TkCanvas::blit 上真窗口。拖拽轨道 / 滚轮缩放 /
# 空格暂停自转 / R 复位；--png 无头出图（CI 可验证，无需桌面）。
add_executable(hypercube_4d_demo example/hypercube_4d_demo.cpp)
target_link_libraries(hypercube_4d_demo PRIVATE termchart_gui_win32)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hypercube_4d_demo PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
set_target_properties(hypercube_4d_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")
