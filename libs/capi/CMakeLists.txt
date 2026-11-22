# libs/capi/CMakeLists.txt —— termchart_capi 共享库（C ABI 适配层）
#
# 产出：termchart_capi 共享库，链接静态 termchart（故单 DLL 自包含，ctypes 直接加载）。
# 导出：tc_* 为 extern "C"，MinGW 自动导出，无需 declspec。
# 门控：仅当 TERMCHART_BUILD_CAPI=ON 时由顶层 CMakeLists add_subdirectory 进入。
add_library(termchart_capi SHARED src/capi.cpp)
add_library(termchart::capi ALIAS termchart_capi)

# 静态版：给进程内绑定（Python .pyd）用。同一份源码编两种形态，模型与行为完全一致；
# 静态链接让 wheel 里的扩展自包含，不必依赖运行时的 termchart_capi.dll。
add_library(termchart_capi_static STATIC src/capi.cpp)
add_library(termchart::capi_static ALIAS termchart_capi_static)
target_include_directories(termchart_capi_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(termchart_capi_static PRIVATE termchart::termchart)
if(WIN32)
    target_link_options(termchart_capi_static PRIVATE -static-libgcc -static-libstdc++)
endif()

target_include_directories(termchart_capi
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(termchart_capi PRIVATE termchart::termchart)

# Windows 下静态链接 MinGW 运行时，使 DLL 自包含（与 pybind11 模块同策略）。
if(WIN32)
    target_link_options(termchart_capi PRIVATE -static-libgcc -static-libstdc++)
endif()

# 打包进 wheel / 安装：bindings/python 的 ctypes 包在运行时加载它。
install(TARGETS termchart_capi
        LIBRARY DESTINATION .
        RUNTIME DESTINATION .
        ARCHIVE DESTINATION .)
