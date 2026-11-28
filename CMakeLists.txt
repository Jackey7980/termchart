cmake_minimum_required(VERSION 3.25)

project(termchart
        VERSION 1.0.0
        DESCRIPTION "Modern C++20 terminal charting library with zero mandatory dependencies"
        LANGUAGES CXX)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# ── 全局开关与规则源 ──────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(TermchartModules)      # 分层规则唯一源（LAYER / NARROW / FORBIDDEN）
include(TermchartOptions)      # 模块与依赖开关
include(CompilerWarnings)
include(Sanitizers)
include(TermchartAbiVersion)
include(FindTermchartDeps)

# 静态优先：v0.1 只承诺源码级与静态库使用（见 ABI_POLICY.md）。
if(TERMCHART_BUILD_SHARED)
    set(BUILD_SHARED_LIBS ON)
endif()

# ── 库目标 ────────────────────────────────────────────────────────────────
file(GLOB_RECURSE TERMCHART_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
# WASM 胶水（src/wasm/termchart_wasm.cpp）只在 Emscripten 工具链下编（见下方
# termchart_wasm 目标），不能进桌面/CI 的库构建——它 #include <emscripten/...>。
list(FILTER TERMCHART_SOURCES EXCLUDE REGEX "src/wasm/")

add_library(termchart ${TERMCHART_SOURCES})
add_library(termchart::termchart ALIAS termchart)

target_include_directories(termchart
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}   # src/term/platform_internal.hpp 的 "src/..." 路径
)

target_compile_features(termchart PUBLIC cxx_std_20)
set_target_properties(termchart PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    VERSION ${PROJECT_VERSION}
    SOVERSION ${TERMCHART_ABI_VERSION}
    POSITION_INDEPENDENT_CODE ON
)

# Windows DLL 只导出显式 __declspec(dllexport) 的符号。库里有大量「定义在
# src/*.cpp、声明在公开头」的入口没有逐个标 TERMCHART_API —— 静态构建下该宏为
# 空，问题从不暴露。手工给上百个声明补标记既无法靠类型系统验证，又会把内部
# 实现符号误当 ABI 承诺（本库只承诺 PIMPL 门面，见 ABI_POLICY.md）。改用
# 由链接器导出全库符号；TERMCHART_API 只保留「显式 ABI 承诺」语义。
# 实测 CMake 的 WINDOWS_EXPORT_ALL_SYMBOLS 在本机 MinGW 不生效（不生成 .def，
# DLL 仍只导出显式 dllexport 的成员函数，构造/析构缺失 → 链接期 undefined
# reference），故用 GNU ld 的 --export-all-symbols。2026-11-13 实测修正。
if(TERMCHART_BUILD_SHARED AND WIN32)
    target_link_options(termchart PRIVATE -Wl,--export-all-symbols)
endif()

# network/socket_win32.cpp 的 Winsock2 符号与 network/tls/schannel.cpp 的
# SChannel/证书 API 来自操作系统库——ws2_32 / schannel / secur32 / crypt32
# 是平台系统库而非第三方依赖，"零强制依赖"口径不变。
# proxy_sys_win32.cpp 读注册表用 advapi32（同样系统库）。
if(WIN32)
    target_link_libraries(termchart PRIVATE ws2_32 schannel secur32 crypt32 advapi32)
endif()

# proxy_sys_posix.cpp 在 macOS 上读系统代理配置（系统框架，非第三方）：
# SystemConfiguration 提供 SCDynamicStore，CoreFoundation 提供
# CFDictionary/CFString/CFNumber 等基础类型（SystemConfiguration 的头文件
# 引用它们，但框架本身不传递符号，必须显式链接，否则链接期 undefined）。
if(APPLE)
    target_link_libraries(termchart PRIVATE "-framework SystemConfiguration"
                                             "-framework CoreFoundation")
endif()

if(NOT TERMCHART_BUILD_SHARED)
    target_compile_definitions(termchart PUBLIC TERMCHART_STATIC)
else()
    target_compile_definitions(termchart PRIVATE TERMCHART_BUILD_SHARED)
endif()

termchart_set_warnings(termchart)
termchart_apply_sanitizers(termchart)

# 可选依赖（缺依赖只降级，不失败）—— 见 cmake/FindTermchartDeps.cmake
if(TERMCHART_HAVE_LIBPNG)
    target_link_libraries(termchart PRIVATE PNG::PNG)
    target_compile_definitions(termchart PRIVATE TERMCHART_HAVE_LIBPNG=1)
endif()
if(TERMCHART_HAVE_ZLIB)
    target_link_libraries(termchart PRIVATE ZLIB::ZLIB)
    target_compile_definitions(termchart PRIVATE TERMCHART_HAVE_ZLIB=1)
endif()
if(TERMCHART_HAVE_UTF8PROC)
    target_link_libraries(termchart PRIVATE utf8proc::utf8proc)
    target_compile_definitions(termchart PRIVATE TERMCHART_HAVE_UTF8PROC=1)
endif()

# vendored stb_image（可选）：JPEG/GIF/WebP 解码。头在 third_party/stb；实现 TU
# src/media/image/stb_image_impl.cpp 用 STB_IMAGE_IMPLEMENTATION 单独编一份，受
# TERMCHART_HAVE_STB_IMAGE 守卫（关闭时该 TU 为空，零代价、不引入符号）。
if(TERMCHART_HAVE_STB_IMAGE)
    target_include_directories(termchart SYSTEM PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb)
    target_compile_definitions(termchart PRIVATE TERMCHART_HAVE_STB_IMAGE=1)
endif()

# ── GUI 平行栈库（header-only，链接 termchart；复用 scene/color/image）──
# 平行栈（widget_tree / native_loop / layout / backend_tk）与终端栈共享到
# scene(L4) 为止，见 docs/design/gui_stack.md。渲染核心先期落地在 termchart::gui。
add_library(termchart_gui INTERFACE)
add_library(termchart::termchart_gui ALIAS termchart_gui)
target_include_directories(termchart_gui INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(termchart_gui INTERFACE termchart::termchart)

# 测试框架必须在所有含 add_test 的子目录（含可选 GUI 插件，其回归测试也用 add_test）
# 之前开启；否则子目录里的 add_test 会被静默忽略。
if(TERMCHART_BUILD_TESTS)
    enable_testing()
endif()

# 原生 Win32 GUI 后端（可选插件，默认关）：接上即可开真实窗口；不进核心零依赖构建。
if(TERMCHART_BUILD_GUI_WIN32 AND WIN32)
    add_subdirectory(plugins/gui_win32)
endif()

# 原生 Qt GUI 后端（可选插件，默认关）：Qt5/Qt6 Widgets，找不到即配置期报错。
if(TERMCHART_BUILD_GUI_QT)
    add_subdirectory(plugins/gui_qt)
endif()

# 原生 Tk GUI 后端（可选插件，默认关）：Tcl/Tk C API，找不到即配置期报错。
if(TERMCHART_BUILD_GUI_TK)
    add_subdirectory(plugins/gui_tk)
endif()

# 原生 X11 GUI 后端（Take 2 句柄注入，Unix 桌面）：插件即可选，按平台自动纳入；
# 宿主缺 libX11 开发库时插件自己跳过（不拖累核心构建）。
if(UNIX AND NOT APPLE)
    add_subdirectory(plugins/gui_x11)
endif()

# 原生 Cocoa GUI 后端（Take 2 NSView 注入）：仅 macOS，按平台自动纳入。
if(APPLE)
    add_subdirectory(plugins/gui_cocoa)
endif()

# JNI 后端（Take 2 句柄注入）：Android / 桌面 Java；有 JDK 即构建，缺失时插件自己跳过。
add_subdirectory(plugins/gui_jni)

# Emscripten WASM 目标（可选，需 emsdk；默认关、不进 CI）。把 image::Bitmap 推到网页
# <canvas>，让库能在浏览器里出真彩图。仅当 Emscripten 工具链生效且显式开启时才编。
if(EMSCRIPTEN AND TERMCHART_BUILD_WASM)
    add_executable(termchart_wasm src/wasm/termchart_wasm.cpp)
    target_link_libraries(termchart_wasm PRIVATE termchart)
    # 导出清单必须用「紧连 + 逗号分隔」写法，不能写 "SHELL:-s EXPORTED_FUNCTIONS=['_a','_b']"：
    # 在 Windows + Ninja 下那层单引号会被 shell 剥掉，emcc 收到 [_a,_b] 解析失败后**静默忽略**，
    # 最终 wasm 里一个自定义导出都没有（真踩过，只在 Linux 上跑的 CI 发现不了）。
    target_link_options(termchart_wasm PRIVATE
        "-sEXPORTED_FUNCTIONS=_termchart_blit_canvas,_termchart_render_demo"
        "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap"
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/examples/wasm/shell.html")
    # --shell-file 只在输出 HTML 时生效；add_executable 在 Emscripten 下默认后缀是 .js，
    # 不改后缀的话 shell 模板会被静默忽略（em++ 仅给一条 -Wunused-command-line-argument）。
    set_target_properties(termchart_wasm PROPERTIES SUFFIX ".html")
endif()

# Python 绑定复用 C ABI 的通用图表模型（tc_chart_create(kind) + 数据形状入口 +
# 属性通道 + SVG/PNG 导出），否则 Python 侧要按图种手写一遍包装——那正是
# "牵一发而动全身"。因此 PYTHON=ON 时自动启用 CAPI（源码在仓库内，无外部依赖）。
if(TERMCHART_BUILD_PYTHON AND NOT TERMCHART_BUILD_CAPI)
    set(TERMCHART_BUILD_CAPI ON CACHE BOOL "构建 C ABI 适配层 termchart_capi（Python 绑定前置）" FORCE)
    message(STATUS "TERMCHART_BUILD_PYTHON=ON：已自动启用 TERMCHART_BUILD_CAPI（Python 绑定复用 C ABI 模型）")
endif()

# C ABI 适配层（可选插件，默认关）：termchart_capi 共享库 + 静态库，链接 termchart；
# 是 Python / Rust / Java 桥的共同前置（见 docs/design/ffi_plan.md §1）。
if(TERMCHART_BUILD_CAPI)
    add_subdirectory(libs/capi)
endif()

# Python 绑定（可选插件，默认关）：pybind11 模块，链接 termchart 与 capi 静态库；
# 缺 pybind11 即报错。
if(TERMCHART_BUILD_PYTHON)
    add_subdirectory(bindings/python)
endif()

# Java 高层出图桥（可选插件，默认关）：JNI glue 包 termchart_capi；需 CAPI + JDK。
if(TERMCHART_BUILD_JAVA)
    if(NOT TERMCHART_BUILD_CAPI)
        message(FATAL_ERROR "TERMCHART_BUILD_JAVA=ON 需要 TERMCHART_BUILD_CAPI=ON（共同前置）")
    endif()
    add_subdirectory(bindings/java)
endif()

# ── 测试与示例 ────────────────────────────────────────────────────────────
if(TERMCHART_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(TERMCHART_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# 综合能力展示（仓库根新建目录 showcase/）：TUI 全平台 + GUI 仅 Win32 插件
add_subdirectory(showcase)

# 性能基线：零外部依赖、编译快；`--quick` 档进 ctest 冒烟（见 bench/CMakeLists.txt）。
if(TERMCHART_BUILD_BENCH)
    add_subdirectory(bench)
endif()

# ── 生成规则与校验目标（护栏入口） ─────────────────────────────────────────
include(TermchartGenerate)   # 数据表 / 附一 / 附四 / 单头 + check_* 目标
include(TermchartSingleHeader)

# ── 安装与导出 ────────────────────────────────────────────────────────────
include(GNUInstallDirs)   # CMAKE_INSTALL_{LIBDIR,BINDIR,INCLUDEDIR} 的唯一来源
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/TermchartConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/termchartConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/termchart"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/termchartConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(TARGETS termchart
        EXPORT termchartTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# 离线地图数据（assets/maps/*.json）：随安装包分发，供 GeoMap 免联网取用。
install(DIRECTORY assets/maps DESTINATION ${CMAKE_INSTALL_DATADIR}/termchart)

install(EXPORT termchartTargets
        FILE termchartTargets.cmake
        NAMESPACE termchart::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/termchart)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/termchartConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/termchartConfigVersion.cmake"
        "${CMAKE_CURRENT_LIST_DIR}/cmake/TermchartModules.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/termchart)
