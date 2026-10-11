cmake_minimum_required(VERSION 3.25)

project(termchart
        VERSION 0.1.0
        DESCRIPTION "Modern C++20 terminal charting library with zero mandatory dependencies"
        LANGUAGES CXX)

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

# ── 测试与示例 ────────────────────────────────────────────────────────────
if(TERMCHART_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(TERMCHART_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

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
