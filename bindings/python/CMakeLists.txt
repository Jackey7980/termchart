# bindings/python/CMakeLists.txt —— pybind11 Python 绑定（可选插件）
#
# 构建规则：
#   * 默认 TERMCHART_BUILD_PYTHON=OFF
#   * 开启后调用 find_package(pybind11 QUIET)；缺失时 **fail-loud**
#     （用户明确要求"可选扩展显式报错不静默降级"，故不复用
#      FindTermchartDeps 的"缺依赖只降级"约定）
#   * 产出 Python 模块 `termchart`，链接 termchart::termchart

option(TERMCHART_BUILD_PYTHON "构建 Python 绑定（pybind11，可选）" OFF)

if(NOT TERCHART_BUILD_PYTHON)
    return()
endif()

find_package(pybind11 REQUIRED)

pybind11_add_module(termchart_pybind
    termchart_pybind.cpp
)

target_link_libraries(termchart_pybind PRIVATE termchart::termchart)
target_compile_definitions(termchart_pybind PRIVATE TERMCHART_HAVE_PYBIND11=1)
termchart_set_warnings(termchart_pybind)

# Python 用户从 site-packages 拿到 termchart.so 后可 `import termchart`
