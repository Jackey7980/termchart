// bindings/python/termchart_pybind.cpp —— pybind11 模块入口
//
// 默认不构建（cmake 选项 TERMCHART_BUILD_PYTHON=OFF），构建需：
//   * find_package(pybind11 QUIET) 成功
//   * 编译期定义 TERMCHART_HAVE_PYBIND11
//
// 暴露 API（薄壳，与 C++ 高层入口一一对应，不重新设计）：
//   termchart.plot(x, y, title="")          ← api::render_plot
//   termchart.scatter(x, y, title="")       ← api::render_scatter
//   termchart.bar(labels, values, title="") ← api::render_bar
//   termchart.version()                     ← TERMCHART_VERSION_STRING
//
// 故意不暴露：
//   * plot/scatter/bar（写 stdout 的变体；Python 端该走 render_* 拿字符串）
//   * 所有 chart 子类（按用户的"一行出图 + 高级扩展点最后露面"取向）

#ifdef TERMCHART_HAVE_PYBIND11

#include <stdexcept>
#include <string>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "termchart/api/quick.hpp"
#include "termchart/version.hpp"

namespace py = pybind11;

namespace {

termchart::api::PlotOpts make_opts(const std::string& title) {
    termchart::api::PlotOpts o;
    o.title = title;
    return o;
}

std::string py_plot(const std::vector<double>& x, const std::vector<double>& y,
                    const std::string& title) {
    auto r = termchart::api::render_plot(x, y, make_opts(title));
    if (!r.has_value()) throw std::runtime_error("termchart: render_plot failed");
    return std::move(r.value());
}

std::string py_scatter(const std::vector<double>& x, const std::vector<double>& y,
                       const std::string& title) {
    auto r = termchart::api::render_scatter(x, y, make_opts(title));
    if (!r.has_value()) throw std::runtime_error("termchart: render_scatter failed");
    return std::move(r.value());
}

std::string py_bar(const std::vector<std::string>& labels,
                   const std::vector<double>& values, const std::string& title) {
    auto r = termchart::api::render_bar(values, labels, make_opts(title));
    if (!r.has_value()) throw std::runtime_error("termchart: render_bar failed");
    return std::move(r.value());
}

}  // namespace

PYBIND11_MODULE(termchart, m) {
    m.doc() = "termchart Python bindings (one-liner plotting API)";
    m.def("version", []() { return std::string(TERMCHART_VERSION_STRING); },
          "termchart version string");

    m.def("plot",    &py_plot,    py::arg("x"), py::arg("y"),
          py::arg("title") = std::string{},
          "Plot a line chart. Returns the rendered ANSI text (terminal cells).");
    m.def("scatter", &py_scatter, py::arg("x"), py::arg("y"),
          py::arg("title") = std::string{},
          "Plot a scatter chart.");
    m.def("bar",     &py_bar,     py::arg("labels"), py::arg("values"),
          py::arg("title") = std::string{},
          "Plot a bar chart.");
}

#endif  // TERMCHART_HAVE_PYBIND11
