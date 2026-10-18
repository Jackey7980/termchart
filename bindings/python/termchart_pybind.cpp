// bindings/python/termchart_pybind.cpp —— pybind11 模块入口
//
// 默认不构建（cmake 选项 TERMCHART_BUILD_PYTHON=OFF），构建需：
//   * find_package(pybind11 QUIET) 成功
//   * 编译期定义 TERMCHART_HAVE_PYBIND11
//
// 暴露 API（薄壳，与 C++ 一一对应，不重新设计）：
//   termchart.plot(x, y, title="")          ← quick::plot
//   termchart.scatter(x, y, title="")       ← quick::scatter
//   termchart.bar(labels, values, title="") ← quick::bar
//   termchart.version()                     ← "0.1.0"
//
// 故意不暴露：
//   * render_chart / print_chart（依赖 stdout；Python 端该走 plot/scatter/bar）
//   * 所有 chart 子类（按用户的"一行出图 + 高级扩展点最后露面"取向）
//
// 编译：见 bindings/python/CMakeLists.txt（外部用户通过 pip install 路径消费）

#ifdef TERMCHART_HAVE_PYBIND11

#include <string>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "termchart/api/quick.hpp"
#include "termchart/version.hpp"

namespace py = pybind11;

namespace {

std::string py_plot(const std::vector<double>& x, const std::vector<double>& y,
                    const std::string& title) {
    return termchart::api::quick::plot(x, y, title);
}

std::string py_scatter(const std::vector<double>& x, const std::vector<double>& y,
                       const std::string& title) {
    return termchart::api::quick::scatter(x, y, title);
}

std::string py_bar(const std::vector<std::string>& labels,
                   const std::vector<double>& values,
                   const std::string& title) {
    return termchart::api::quick::bar(labels, values, title);
}

}  // namespace

PYBIND11_MODULE(termchart, m) {
    m.doc() = "termchart Python bindings (one-liner plotting API)";
    m.def("version", &termchart::version, "termchart version string");

    m.def("plot",    &py_plot,    py::arg("x"), py::arg("y"),
          py::arg("title") = std::string{},
          "Plot a line chart. Returns the rendered text (terminal cells).");
    m.def("scatter", &py_scatter, py::arg("x"), py::arg("y"),
          py::arg("title") = std::string{},
          "Plot a scatter chart.");
    m.def("bar",     &py_bar,     py::arg("labels"), py::arg("values"),
          py::arg("title") = std::string{},
          "Plot a bar chart.");
}

#endif  // TERMCHART_HAVE_PYBIND11
