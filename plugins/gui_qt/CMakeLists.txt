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

# ---- LGPL 合规闸门 ----
# Qt 采用 LGPLv3 / GPLv3 / 商业 三重授权，而 termchart 本体是宽松许可（见 LICENSE）。
# 只有**动态链接** LGPLv3 的 Qt 才满足其 §4「终端用户可替换库」的要求（保留 Qt 的
# DLL/so，用户能自行换一个 Qt 而不必重编译本事）；反之若链接**静态构建**的 Qt，
# LGPLv3 §4(d) 的「安装信息 / 可重链接」义务会转嫁给每一个下游分发者——这与本项目
# 的宽松定位直接冲突。故在此 fail-loud 拦下静态 Qt，而不是靠文档提醒（与「可选扩展
# 显式报错、不静默回退」的既定取向一致）。确需静态链接请自行取得 Qt 商业授权。
get_target_property(_termchart_qt_core_type ${TERMCHART_QT_VERSION_LABEL}::Core TYPE)
if(_termchart_qt_core_type STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR
        "检测到静态构建的 Qt（${TERMCHART_QT_VERSION_LABEL}::Core 为 STATIC_LIBRARY）。\n"
        "termchart 本体是宽松许可，静态链接 LGPLv3 的 Qt 会把 §4(d) 的可重链接义务\n"
        "转嫁给所有下游分发者。请改用 Qt 的动态构建（官方安装包默认即是），\n"
        "或为 Qt 取得商业授权后再开启 TERMCHART_BUILD_GUI_QT。")
endif()
unset(_termchart_qt_core_type)

message(STATUS "termchart GUI Qt 后端: ${TERMCHART_QT_VERSION_LABEL} Widgets（动态链接，LGPLv3 合规）")

add_library(termchart_gui_qt STATIC src/qt_backend.cpp)
target_include_directories(termchart_gui_qt PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
# PUBLIC 传 termchart_gui（header-only）→ 再传 termchart（实现 Bitmap/字体等符号）。
target_link_libraries(termchart_gui_qt PUBLIC termchart_gui PRIVATE ${TERMCHART_QT_TARGET})
set_target_properties(termchart_gui_qt PROPERTIES
    AUTOMOC ON  # 保留 AUTOMOC：CanvasWidget 目前无 Q_OBJECT（事件走 std::function 回调），
                # 一旦将来加信号/槽即自动生效；无 Q_OBJECT 时它不做任何事。
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

# ---- 回归测试（仅在本插件被构建时存在）----
# Qt 运行时不在版本库内（LGPL 动态链接、二进制体积大，不 vendoring）。测试环境由本文件
# 注入，确保可复现且不依赖调用方的全局 PATH：
#  - 把 Qt 的 bin 目录提到 PATH 最前，使其自带 libstdc++ 优先于其它 MinGW，规避 GCC
#    版本错配导致的 STATUS_ENTRYPOINT_NOT_FOUND（0xC0000139）；
#  - 设 QT_QPA_PLATFORM=offscreen 与插件路径，便于无显示器 CI 复现。
# 二进制如何可复现地获取见 docs/guide/gui_qt.md 与 tools/setup_qt_ucrt.ps1。
get_target_property(_qt_core_loc ${TERMCHART_QT_VERSION_LABEL}::Core LOCATION)
get_filename_component(_qt_bin_dir "${_qt_core_loc}" DIRECTORY)
# Qt 插件目录布局因发行方而异：官方 Qt.io 装到 <prefix>/plugins（与 bin 同级），
# MSYS2（UCRT）装到 <prefix>/share/qt6/plugins。两种都试，挑真实含 platforms 子目录的。
get_filename_component(_qt_prefix "${_qt_bin_dir}/.." ABSOLUTE)
set(_qt_plugins_candidates
    "${_qt_prefix}/plugins"
    "${_qt_prefix}/share/qt6/plugins"
    "${_qt_prefix}/lib/qt6/plugins")
set(_qt_plugins_dir "")
foreach(_cand IN LISTS _qt_plugins_candidates)
    if(EXISTS "${_cand}/platforms")
        set(_qt_plugins_dir "${_cand}")
        break()
    endif()
endforeach()
if(_qt_plugins_dir STREQUAL "")
    # 兜底：沿用历史推测，避免配置期直接失败（运行期若缺 offscreen 插件会自行报错）。
    set(_qt_plugins_dir "${_qt_prefix}/plugins")
endif()

add_executable(qt_backend_test test/test_qt_backend.cpp)
target_link_libraries(qt_backend_test PRIVATE termchart_gui_qt)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(qt_backend_test PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# 注意：测试 PATH 只放 Qt 的 bin（内含 Qt DLL 与配套 libstdc++），**不**继承调用方的全局
# PATH——否则可能把其它 MinGW/旧版（msvcrt）Qt 的 bin 一并带进来，导致加载错版 Qt6Core
# 而回到「UCRT exe 混 msvcrt Qt」的堆损坏（0xC0000374）。Qt 运行所需系统 DLL 由 OS 直接解析，
# 无需额外 PATH。
add_test(NAME qt_backend_test COMMAND qt_backend_test)
set_tests_properties(qt_backend_test PROPERTIES
    ENVIRONMENT "PATH=${_qt_bin_dir};QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${_qt_plugins_dir}")

add_test(NAME qt_demo_smoke COMMAND qt_demo --smoke)
set_tests_properties(qt_demo_smoke PROPERTIES
    ENVIRONMENT "PATH=${_qt_bin_dir};QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${_qt_plugins_dir}")
