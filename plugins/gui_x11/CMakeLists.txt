# plugins/gui_x11/CMakeLists.txt —— 原生 X11 GUI 后端（可选插件，Take 2 句柄注入）
#
# 由顶层在 Unix（非 Apple）下自动 add_subdirectory。静态库 + 演示。
# 依赖 Xlib（libX11 开发包）；缺开发库时本插件自行跳过，不拖累核心零依赖构建。
find_package(X11 QUIET)
if(NOT X11_FOUND)
    message(STATUS "termchart GUI X11 插件: 未找到 libX11 开发库，跳过")
    return()
endif()

message(STATUS "termchart GUI X11 后端: ${X11_LIBRARIES}")

add_library(termchart_gui_x11 STATIC src/x11_backend.cpp)
target_include_directories(termchart_gui_x11 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${X11_INCLUDE_DIR})
target_link_libraries(termchart_gui_x11 PUBLIC termchart_gui PRIVATE ${X11_LIBRARIES})

# 平台胶水层用轻量告警（与 gui_win32/gui_tk 同口径）。
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(termchart_gui_x11 PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

add_executable(x11_demo example/x11_demo.cpp)
target_link_libraries(x11_demo PRIVATE termchart_gui_x11 ${X11_LIBRARIES})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(x11_demo PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
set_target_properties(x11_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")

# 冒烟：本机有 DISPLAY 直接跑，无显示但装了 Xvfb 就用 xvfb-run 起虚拟屏；
# 两者都没有（例如 CI 精简镜像）则明确跳过，而不是让测试因缺 DISPLAY 而假失败。
if(TERMCHART_BUILD_TESTS)
    find_program(TERMCHART_XVFB_RUN xvfb-run)
    if(TERMCHART_XVFB_RUN)
        add_test(NAME x11_smoke
            COMMAND ${TERMCHART_XVFB_RUN} -a -s "-screen 0 800x600x24"
                    $<TARGET_FILE:x11_demo> --smoke)
        set_tests_properties(x11_smoke PROPERTIES TIMEOUT 30)
    elseif(NOT "$ENV{DISPLAY}" STREQUAL "")
        add_test(NAME x11_smoke COMMAND x11_demo --smoke)
        set_tests_properties(x11_smoke PROPERTIES TIMEOUT 30)
    else()
        message(STATUS "termchart GUI X11 插件: 无 DISPLAY 且无 xvfb-run，跳过 x11_smoke 测试")
    endif()
endif()
