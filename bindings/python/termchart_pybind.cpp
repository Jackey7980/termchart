// bindings/python/termchart_pybind.cpp —— pybind11 模块入口
//
// 默认不构建（cmake 选项 TERMCHART_BUILD_PYTHON=OFF），构建需：
//   * find_package(pybind11 REQUIRED) 成功
//   * 编译期定义 TERMCHART_HAVE_PYBIND11
//
// 设计取向（对齐 api::quick 的"一行出图"）：
//   新手默认路径**零配置**——`tc.plot(x, y)` 直接拿到可打印的 ANSI 文本；
//   要文件就 `tc.save_png(x, y, "a.png")`；底层（Chart / Scene / Backend）
//   **不**暴露给 Python 用户，那些留给 C++ 侧。
//
// 暴露 API：
//   termchart.version()                        ← TERMCHART_VERSION_STRING
//   termchart.plot(x, y, ...)                  ← ANSI 文本（折线）
//   termchart.scatter(x, y, ...)               ← ANSI 文本（散点）
//   termchart.bar(labels, values, ...)         ← ANSI 文本（柱状）
//   termchart.plot_svg(x, y, ...)              ← SVG 文本
//   termchart.scatter_svg / bar_svg            ← SVG 文本
//   termchart.save_svg(x, y, path, ...)        ← 写 SVG 文件
//   termchart.save_png(x, y, path, ...)        ← 写 PNG 文件（Scene → Bitmap → PNG）
//   termchart.render_line(text, ...)           ← 带样式的一行文本
//
// 故意不暴露：
//   * 所有 chart 子类（按用户的"一行出图 + 高级扩展点最后露面"取向）；
//   * 交互层（终端鼠标 / 键盘）—— Python 侧没有事件循环可言；
//   * 后端注册表（终端用户不该关心"用哪个 backend"）。

#ifdef TERMCHART_HAVE_PYBIND11

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "termchart/api/chart.hpp"
#include "termchart/api/export_image.hpp"
#include "termchart/api/opts.hpp"
#include "termchart/api/quick.hpp"
#include "termchart/chart/cartesian/bar_chart.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/chart/cartesian/scatter_chart.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/gui/scene_renderer.hpp"
#include "termchart/image/encode.hpp"
#include "termchart/version.hpp"

namespace py = pybind11;

namespace {

using termchart::Point;
using termchart::chart::Chart;

constexpr std::uint32_t kDefaultWidth = 80;
constexpr std::uint32_t kDefaultHeight = 24;
constexpr std::uint32_t kDefaultPixelWidth = 960;
constexpr std::uint32_t kDefaultPixelHeight = 540;

// (x[i], y[i]) → Series 数据点；长度取两者较小者（与 api::quick 同一口径）。
std::vector<Point> to_points(const std::vector<double>& x, const std::vector<double>& y) {
    const std::size_t count = std::min(x.size(), y.size());
    std::vector<Point> points;
    points.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        points.push_back(Point{x[i], y[i]});
    }
    return points;
}

std::unique_ptr<Chart> make_line(const std::vector<double>& x, const std::vector<double>& y,
                                 const std::string& title) {
    auto chart = std::make_unique<termchart::chart::LineChart>();
    if (!title.empty()) {
        chart->set_title(title);
    }
    termchart::chart::Series series;
    series.name = "y";
    series.color = termchart::chart::default_color(termchart::chart::DefaultColor::Neutral);
    series.points = to_points(x, y);
    chart->add_series(std::move(series));
    return chart;
}

std::unique_ptr<Chart> make_scatter(const std::vector<double>& x, const std::vector<double>& y,
                                    const std::string& title) {
    auto chart = std::make_unique<termchart::chart::ScatterChart>();
    if (!title.empty()) {
        chart->set_title(title);
    }
    termchart::chart::Series series;
    series.name = "y";
    series.color = termchart::chart::default_color(termchart::chart::DefaultColor::Accent);
    series.points = to_points(x, y);
    chart->add_series(std::move(series));
    return chart;
}

std::unique_ptr<Chart> make_bar(const std::vector<double>& values,
                                const std::vector<std::string>& labels,
                                const std::string& title) {
    auto chart = std::make_unique<termchart::chart::BarChart>();
    if (!title.empty()) {
        chart->set_title(title);
    }
    std::vector<std::string> names = labels;
    if (names.size() < values.size()) {  // 缺省按序号补标签
        names.reserve(values.size());
        for (std::size_t i = names.size(); i < values.size(); ++i) {
            names.push_back(std::to_string(i));
        }
    }
    chart->set_values(values, names,
                      termchart::chart::default_color(termchart::chart::DefaultColor::Accent),
                      "series");
    return chart;
}

termchart::api::ApiOptions make_opts(std::uint32_t width, std::uint32_t height) {
    termchart::api::ApiOptions opts;
    opts.width(static_cast<std::uint16_t>(width));
    opts.height(static_cast<std::uint16_t>(height));
    opts.newline(false);
    return opts;
}

// 渲染成 ANSI 文本（不写 stdout）。
std::string to_ansi(Chart& chart, std::uint32_t width, std::uint32_t height) {
    const termchart::api::ApiOptions opts = make_opts(width, height);
    const auto rendered = termchart::api::render_chart(chart, opts);
    if (!rendered.has_value()) {
        throw std::runtime_error("termchart: render_chart failed");
    }
    return rendered.value();
}

// 渲染成 SVG 文本。
std::string to_svg(const Chart& chart, std::uint32_t width, std::uint32_t height) {
    const auto rendered = termchart::api::chart_to_svg(chart, width, height);
    if (!rendered.has_value()) {
        throw std::runtime_error("termchart: chart_to_svg failed（该图种尚未迁移到 Scene IR）");
    }
    return rendered.value();
}

// 渲染成 PNG 并写盘：chart → Scene → Bitmap → PNG。
void to_png_file(const Chart& chart, const std::string& path, std::uint32_t width,
                 std::uint32_t height, std::uint32_t pixel_width, std::uint32_t pixel_height) {
    const auto scene = termchart::api::chart_to_scene(chart, width, height);
    if (!scene.has_value()) {
        throw std::runtime_error("termchart: chart_to_scene failed（该图种尚未迁移到 Scene IR）");
    }
    termchart::gui::RenderOptions render_options;
    render_options.width = pixel_width;
    render_options.height = pixel_height;
    termchart::gui::SceneRenderer renderer(render_options);
    const termchart::image::Bitmap bitmap = renderer.render(*scene);
    const termchart::Status status =
        termchart::api::save_image(bitmap, path, termchart::image::ImageFormat::Png);
    if (status != termchart::ErrorCode::Ok) {
        throw std::runtime_error("termchart: save_png failed（编码或写盘出错）");
    }
}

void write_text_file(const std::string& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("termchart: cannot open file: " + path);
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!stream) {
        throw std::runtime_error("termchart: cannot write file: " + path);
    }
}

// ── pybind 入口 ──────────────────────────────────────────────────────────

std::string py_plot(const std::vector<double>& x, const std::vector<double>& y,
                    const std::string& title, std::uint32_t width, std::uint32_t height) {
    auto chart = make_line(x, y, title);
    return to_ansi(*chart, width, height);
}

std::string py_scatter(const std::vector<double>& x, const std::vector<double>& y,
                       const std::string& title, std::uint32_t width, std::uint32_t height) {
    auto chart = make_scatter(x, y, title);
    return to_ansi(*chart, width, height);
}

std::string py_bar(const std::vector<std::string>& labels, const std::vector<double>& values,
                   const std::string& title, std::uint32_t width, std::uint32_t height) {
    auto chart = make_bar(values, labels, title);
    return to_ansi(*chart, width, height);
}

std::string py_plot_svg(const std::vector<double>& x, const std::vector<double>& y,
                        const std::string& title, std::uint32_t width, std::uint32_t height) {
    auto chart = make_line(x, y, title);
    return to_svg(*chart, width, height);
}

std::string py_scatter_svg(const std::vector<double>& x, const std::vector<double>& y,
                           const std::string& title, std::uint32_t width, std::uint32_t height) {
    auto chart = make_scatter(x, y, title);
    return to_svg(*chart, width, height);
}

std::string py_bar_svg(const std::vector<std::string>& labels, const std::vector<double>& values,
                       const std::string& title, std::uint32_t width, std::uint32_t height) {
    auto chart = make_bar(values, labels, title);
    return to_svg(*chart, width, height);
}

void py_save_svg(const std::vector<double>& x, const std::vector<double>& y,
                 const std::string& path, const std::string& title, std::uint32_t width,
                 std::uint32_t height) {
    auto chart = make_line(x, y, title);
    write_text_file(path, to_svg(*chart, width, height));
}

void py_save_png(const std::vector<double>& x, const std::vector<double>& y,
                 const std::string& path, const std::string& title, std::uint32_t width,
                 std::uint32_t height, std::uint32_t pixel_width, std::uint32_t pixel_height) {
    auto chart = make_line(x, y, title);
    to_png_file(*chart, path, width, height, pixel_width, pixel_height);
}

std::string py_render_line(const std::string& text, std::uint32_t width) {
    termchart::api::ApiOptions opts;
    if (width > 0) {
        opts.width(static_cast<std::uint16_t>(width));
    }
    opts.newline(false);
    const auto rendered = termchart::api::render_line(text, termchart::Style{}, opts);
    if (!rendered.has_value()) {
        throw std::runtime_error("termchart: render_line failed");
    }
    return rendered.value();
}

}  // namespace

PYBIND11_MODULE(termchart, m) {
    m.doc() = "termchart —— zero-dependency terminal charting (matplotlib-style one-liners)";

    m.attr("__version__") = TERMCHART_VERSION_STRING;

    m.def("version", []() { return std::string(TERMCHART_VERSION_STRING); },
          "Return the termchart version string.");

    m.def("plot", &py_plot, py::arg("x"), py::arg("y"), py::arg("title") = std::string{},
          py::arg("width") = kDefaultWidth, py::arg("height") = kDefaultHeight,
          "Draw a line chart and return the ANSI text (terminal cells).");

    m.def("scatter", &py_scatter, py::arg("x"), py::arg("y"), py::arg("title") = std::string{},
          py::arg("width") = kDefaultWidth, py::arg("height") = kDefaultHeight,
          "Draw a scatter chart and return the ANSI text.");

    m.def("bar", &py_bar, py::arg("labels"), py::arg("values"), py::arg("title") = std::string{},
          py::arg("width") = kDefaultWidth, py::arg("height") = kDefaultHeight,
          "Draw a bar chart and return the ANSI text. labels may be empty.");

    m.def("plot_svg", &py_plot_svg, py::arg("x"), py::arg("y"), py::arg("title") = std::string{},
          py::arg("width") = kDefaultWidth, py::arg("height") = kDefaultHeight,
          "Draw a line chart and return the SVG text.");

    m.def("scatter_svg", &py_scatter_svg, py::arg("x"), py::arg("y"),
          py::arg("title") = std::string{}, py::arg("width") = kDefaultWidth,
          py::arg("height") = kDefaultHeight, "Draw a scatter chart and return the SVG text.");

    m.def("bar_svg", &py_bar_svg, py::arg("labels"), py::arg("values"),
          py::arg("title") = std::string{}, py::arg("width") = kDefaultWidth,
          py::arg("height") = kDefaultHeight, "Draw a bar chart and return the SVG text.");

    m.def("save_svg", &py_save_svg, py::arg("x"), py::arg("y"), py::arg("path"),
          py::arg("title") = std::string{}, py::arg("width") = kDefaultWidth,
          py::arg("height") = kDefaultHeight, "Draw a line chart and write an SVG file.");

    m.def("save_png", &py_save_png, py::arg("x"), py::arg("y"), py::arg("path"),
          py::arg("title") = std::string{}, py::arg("width") = kDefaultWidth,
          py::arg("height") = kDefaultHeight, py::arg("pixel_width") = kDefaultPixelWidth,
          py::arg("pixel_height") = kDefaultPixelHeight,
          "Draw a line chart and write a PNG file (chart -> Scene -> bitmap -> PNG).");

    m.def("render_line", &py_render_line, py::arg("text"), py::arg("width") = 0u,
          "Render a piece of text as a styled ANSI terminal line.");
}

#endif  // TERMCHART_HAVE_PYBIND11
