# bindings/python/CMakeLists.txt —— pybind11 Python 绑定（可选插件）
#
# 构建规则：
#   * 选项 TERMCHART_BUILD_PYTHON（定义在 cmake/TermchartOptions.cmake）默认 OFF
#   * 开启后调用 find_package(pybind11 REQUIRED)；缺失时 **fail-loud**
#     （用户明确要求"可选扩展显式报错不静默降级"）
#   * 产出 Python 模块 `termchart`：目标名用 termchart_pybind（避免与核心库目标
#     termchart 冲突），但 OUTPUT_NAME 强制为 termchart，使 `import termchart` 命中文件
#   * 链接 termchart::termchart；Windows 下静态链接 MinGW 运行时，使 .pyd 自包含
#
# 安装规则（pip 可用的关键）：
#   scikit-build-core 把 CMake 的 install 产物打进 wheel，因此**必须**在这里
#   显式 install(TARGETS ...)。少了这一行，模块能编出来但进不了 wheel，
#   `pip install` 之后 `import termchart` 直接 ImportError。
#   DESTINATION "." = wheel 根目录 → site-packages/termchart.<abi>.pyd。

if(NOT TERMCHART_BUILD_PYTHON)
    return()
endif()

find_package(pybind11 REQUIRED)

pybind11_add_module(termchart_pybind
    termchart_pybind.cpp
)
set_target_properties(termchart_pybind PROPERTIES OUTPUT_NAME termchart)

# 链接 C ABI 静态库：Chart(kind) / kinds() / auto_plot 等全量模型直接复用
# tc_* 实现，Python 侧不再按图种手写一遍包装（新增图种自动可用）。
target_link_libraries(termchart_pybind PRIVATE termchart::termchart termchart::capi_static)
target_compile_definitions(termchart_pybind PRIVATE TERMCHART_HAVE_PYBIND11=1)
termchart_set_warnings(termchart_pybind)

# 可选：与 Qt 后端一并构建时，暴露 termchart.show_qt（开 Qt 窗口出图）。
if(TERMCHART_BUILD_GUI_QT)
    target_compile_definitions(termchart_pybind PRIVATE TERMCHART_HAVE_QT=1)
    target_link_libraries(termchart_pybind PRIVATE termchart_gui_qt)
endif()

if(WIN32)
    target_link_options(termchart_pybind PRIVATE -static-libgcc -static-libstdc++)
endif()

# ── 打包进 wheel ─────────────────────────────────────────────────────────
# 模块落在 wheel 根目录（site-packages/termchart.cpython-XXX.pyd）。
install(TARGETS termchart_pybind
        LIBRARY DESTINATION .
        RUNTIME DESTINATION .
        ARCHIVE DESTINATION .)

# 类型存根：让 IDE / mypy 知道 Python 侧的函数签名。
install(FILES "${CMAKE_CURRENT_LIST_DIR}/termchart.pyi" DESTINATION .)
