# plugins/gui_cocoa/CMakeLists.txt —— 原生 Cocoa GUI 后端（可选插件，Take 2 句柄注入）
#
# 由顶层在 macOS 下自动 add_subdirectory。静态库 + 演示。
# 依赖 AppKit/CoreGraphics 系统框架（macOS 自带）；非 Apple 平台直接跳过。
if(NOT APPLE)
    return()
endif()

enable_language(OBJCXX)

find_library(TERMCHART_COCOA_FRAMEWORK Cocoa REQUIRED)
find_library(TERMCHART_COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)

message(STATUS "termchart GUI Cocoa 后端: ${TERMCHART_COCOA_FRAMEWORK}")

add_library(termchart_gui_cocoa STATIC src/cocoa_backend.mm)
target_include_directories(termchart_gui_cocoa PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(termchart_gui_cocoa PUBLIC termchart_gui
    PRIVATE ${TERMCHART_COCOA_FRAMEWORK} ${TERMCHART_COREGRAPHICS_FRAMEWORK})
# .mm 目标需启用 ARC 与块（与宿主一致）；核心是 C++ 不受影响。
target_compile_options(termchart_gui_cocoa PRIVATE -fobjc-arc)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(termchart_gui_cocoa PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

add_executable(cocoa_demo example/cocoa_demo.mm)
target_link_libraries(cocoa_demo PRIVATE termchart_gui_cocoa)
target_compile_options(cocoa_demo PRIVATE -fobjc-arc)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cocoa_demo PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
set_target_properties(cocoa_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")

# CI 无 WindowServer 时用 --offscreen 冒烟（不开窗，仅验证渲染管线）。
if(TERMCHART_BUILD_TESTS)
    add_test(NAME cocoa_offscreen
        COMMAND cocoa_demo --offscreen)
    set_tests_properties(cocoa_offscreen PROPERTIES TIMEOUT 30)
endif()
