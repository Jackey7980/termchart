cmake_minimum_required(VERSION 3.25)

project(termchart
        VERSION 0.1.0
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

# network/socket_win32.cpp 的 Winsock2 符号与 network/tls/schannel.cpp 的
# SChannel/证书 API 来自操作系统库——ws2_32 / schannel / secur32 / crypt32
# 是平台系统库而非第三方依赖，"零强制依赖"口径不变。
# proxy_sys_win32.cpp 读注册表用 advapi32（同样系统库）。
if(WIN32)
    target_link_libraries(termchart PRIVATE ws2_32 schannel secur32 crypt32 advapi32)
endif()

# proxy_sys_posix.cpp 在 macOS 上读 SystemConfiguration（系统框架，非第三方）。
if(APPLE)
    target_link_libraries(termchart PRIVATE "-framework SystemConfiguration")
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
# src/image/stb_image_impl.cpp 用 STB_IMAGE_IMPLEMENTATION 单独编一份，受
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

# Python 绑定（可选插件，默认关）：pybind11 模块，链接 termchart；缺 pybind11 即报错。
if(TERMCHART_BUILD_PYTHON)
    add_subdirectory(bindings/python)
endif()

# ── 测试与示例 ────────────────────────────────────────────────────────────
if(TERMCHART_BUILD_TESTS)
    enable_testing()
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

install(EXPORT termchartTargets
        FILE termchartTargets.cmake
        NAMESPACE termchart::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/termchart)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/termchartConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/termchartConfigVersion.cmake"
        "${CMAKE_CURRENT_LIST_DIR}/cmake/TermchartModules.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/termchart)
