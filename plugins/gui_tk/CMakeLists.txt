# plugins/gui_tk/CMakeLists.txt —— 原生 Tk GUI 后端（可选插件）
#
# 由顶层在 TERMCHART_BUILD_GUI_TK=ON 时 add_subdirectory。静态库 + 一个可运行
# 演示。依赖 Tcl/Tk 开发库（头 tcl.h/tk.h + 链接库 libtk/libtcl）；
# **显式开启但找不到即配置期报错**（fail-loud，不静默跳过——与
# "可选扩展显式报错不静默回退"的既定取向一致）。
#
# 为什么默认 OFF：Tk 是外部依赖（libtk/libtcl 二进制），违背核心
# "零强制依赖、header-only"硬约束。默认构建不碰它。

# 版本化库名探测（Tcl/Tk 8.6 / 9.0 及源码构建的未版本化名）。
find_path(TERMCHART_TCL_INCLUDE_DIR tcl.h
    PATHS
        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Tcl]Installed"  # ActiveTcl 的注册表根
    PATH_SUFFIXES include
)
find_path(TERMCHART_TK_INCLUDE_DIR tk.h)
find_library(TERMCHART_TCL_LIBRARY NAMES tcl tcl90 tcl9.0 tcl86 tcl8.6)
find_library(TERMCHART_TK_LIBRARY NAMES tk tk90 tk9.0 tk86 tk8.6)

if(NOT TERMCHART_TCL_INCLUDE_DIR OR NOT TERMCHART_TK_INCLUDE_DIR
   OR NOT TERMCHART_TCL_LIBRARY OR NOT TERMCHART_TK_LIBRARY)
    message(FATAL_ERROR
        "TERMCHART_BUILD_GUI_TK=ON 但找不到 Tcl/Tk 开发库。\n"
        "请安装 Tcl/Tk 开发包（头 tcl.h/tk.h + 库），并确保 CMAKE_PREFIX_PATH "
        "或 TCL_LIBRARY/TK_LIBRARY 指向其安装根；或关闭该选项（默认 OFF）以跳过 "
        "Tk 后端。")
endif()

message(STATUS "termchart GUI Tk 后端: ${TERMCHART_TK_LIBRARY} + ${TERMCHART_TCL_LIBRARY}")

add_library(termchart_gui_tk STATIC src/tk_backend.cpp)
target_include_directories(termchart_gui_tk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${TERMCHART_TCL_INCLUDE_DIR} ${TERMCHART_TK_INCLUDE_DIR})
# PUBLIC 传 termchart_gui（header-only）→ 再传 termchart（实现 Bitmap/字体等符号）。
target_link_libraries(termchart_gui_tk PUBLIC termchart_gui
    PRIVATE ${TERMCHART_TK_LIBRARY} ${TERMCHART_TCL_LIBRARY})

# Tk 平台层用轻量告警（与 gui_win32/gui_qt 同口径）：不套核心
# termchart_set_warnings（按目标函数，可安全省略），只留 -Wall -Wextra。
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(termchart_gui_tk PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

add_executable(tk_demo example/tk_demo.cpp)
target_link_libraries(tk_demo PRIVATE termchart_gui_tk)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tk_demo PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
set_target_properties(tk_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples"
)
