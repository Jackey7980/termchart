# plugins/gui_qt/CMakeLists.txt —— 原生 Qt GUI 后端（可选插件）
#
# 由顶层在 TERMCHART_BUILD_GUI_QT=ON 时 add_subdirectory。静态库 + 一个可运行
# 演示。依赖 Qt5 或 Qt6 的 Widgets 模块；**显式开启但找不到 Qt 即配置期报错**
# （fail-loud，不静默跳过——与"可选扩展显式报错不静默回退"的既定取向一致）。
#
# 为什么默认 OFF：Qt 是重量级外部依赖（多模块二进制），违背核心"零强制依赖、
# header-only"硬约束。默认构建不碰它；TERMCHART_BUILD_GUI_QT=ON 是显式选择。
include_guard(GLOBAL)

# Qt6 优先，Qt5 兜底（后端 cpp 按 QT_VERSION 分支，两者 API 均支持）。
find_package(Qt6 COMPONENTS Widgets QUIET)
if(Qt6_FOUND)
    set(TERMCHART_QT_TARGET Qt6::Widgets)
    set(TERMCHART_QT_VERSION_LABEL "Qt6")
else()
    find_package(Qt5 COMPONENTS Widgets QUIET)
    if(Qt5_FOUND)
        set(TERMCHART_QT_TARGET Qt5::Widgets)
        set(TERMCHART_QT_VERSION_LABEL "Qt5")
    else()
        message(FATAL_ERROR
            "TERMCHART_BUILD_GUI_QT=ON 但找不到 Qt5/Qt6 的 Widgets 模块。\n"
            "请安装 Qt（Widgets 组件）并确保 CMAKE_PREFIX_PATH 指向其安装根，"
            "或关闭该选项（默认 OFF）以跳过 Qt 后端。")
    endif()
endif()

message(STATUS "termchart GUI Qt 后端: ${TERMCHART_QT_VERSION_LABEL} Widgets")

add_library(termchart_gui_qt STATIC src/qt_backend.cpp)
target_include_directories(termchart_gui_qt PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
# PUBLIC 传 termchart_gui（header-only）→ 再传 termchart（实现 Bitmap/字体等符号）。
target_link_libraries(termchart_gui_qt PUBLIC termchart_gui PRIVATE ${TERMCHART_QT_TARGET})
set_target_properties(termchart_gui_qt PROPERTIES
    AUTOMOC ON  # CanvasWidget 是 QObject  subclass：需要 moc
)

# Qt 平台层的告警策略与 gui_win32 同口径：Qt 头自带大量类型转换告警，
# 不套核心 termchart_set_warnings（按目标函数，可安全省略），只留 -Wall -Wextra。
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(termchart_gui_qt PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

add_executable(qt_demo example/qt_demo.cpp)
target_link_libraries(qt_demo PRIVATE termchart_gui_qt)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(qt_demo PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
set_target_properties(qt_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples"
)
