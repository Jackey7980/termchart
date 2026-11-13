// bindings/python/termchart_pybind.cpp -- pybind11 module entry.
//
// Built only when TERMCHART_BUILD_PYTHON=ON (option default OFF). Needs
// find_package(pybind11 REQUIRED) and TERMCHART_HAVE_PYBIND11 defined.
//
// Orientation (aligned with api::quick's "one-line chart"): the default path is
// zero-config -- tc.plot(x, y) returns ANSI text; tc.save_png(x, y, path) writes
// a PNG. Lower layers (Chart/Scene/Backend) are not exposed to Python users.
//
// Inputs accept plain Python sequences AND numpy arrays (read zero-copy from the
// buffer when contiguous) AND pandas Series/DataFrame columns (via .to_numpy()).
// Output helpers additionally return raw RGB(A) pixels for embedding into a
// Tkinter/PIL surface, and accept pixel buffers back for PNG encoding.

#ifdef TERMCHART_HAVE_PYBIND11

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <pybind11/numpy.h>
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
#include "termchart/image/bitmap.hpp"
#include "termchart/image/encode.hpp"
#include "termchart/version.hpp"

#ifdef TERMCHART_HAVE_QT
#include "termchart_gui_qt/qt_backend.hpp"
#endif

namespace py = pybind11;
namespace {

using termchart::Point;
using termchart::chart::Chart;

constexpr std::uint32_t kDefaultWidth = 80;
constexpr std::uint32_t kDefaultHeight = 24;
constexpr std::uint32_t kDefaultPixelWidth = 960;
constexpr std::uint32_t kDefaultPixelHeight = 540;

// 1-D numeric view over a numpy array / pandas column / sequence.
struct Numpy1D {
    py::array arr;
    const char* ptr = nullptr;
    py::ssize_t stride = 0;
    char kind = 'd';
    py::ssize_t n = 0;
};

Numpy1D numpy_1d(py::object obj) {
    py::object np = py::none();
    try {
        np = py::module_::import("numpy");
    } catch (const py::error_already_set&) {
        np = py::none();
    }

    auto from_array_like = [&](py::object o) -> py::array {
        py::array a = py::array::ensure(o);
        if (!a && np && py::hasattr(o, "to_numpy")) {
            a = py::array::ensure(o.attr("to_numpy")());
        }
        return a;
    };

    py::array arr = from_array_like(obj);
    if (!arr && np) {
        arr = py::array::ensure(np.attr("asarray")(obj));
    }
    if (!arr) {
        throw std::runtime_error("termchart: expected a 1-D numeric array, Series, or sequence");
    }

    py::buffer_info info = arr.request();
    if (info.ndim != 1) {
        throw std::runtime_error("termchart: expected a 1-D array");
    }
    if (info.strides[0] != static_cast<py::ssize_t>(info.itemsize)) {
        if (!np) {
            throw std::runtime_error("termchart: non-contiguous array needs numpy");
        }
        arr = py::array::ensure(np.attr("ascontiguousarray")(arr));
        info = arr.request();
    }

    Numpy1D out;
    out.arr = arr;
    out.ptr = static_cast<const char*>(info.ptr);
    out.stride = info.strides[0];
    out.n = info.shape[0];
    out.kind = info.format.empty() ? 'd' : info.format[0];
    return out;
}

double read_at(const Numpy1D& v, py::ssize_t i) {
    const char* p = v.ptr + v.stride * i;
    switch (v.kind) {
        case 'd': return *reinterpret_cast<const double*>(p);
        case 'f': return static_cast<double>(*reinterpret_cast<const float*>(p));
        case 'e': return static_cast<double>(*reinterpret_cast<const std::uint16_t*>(p));
        case 'b': return static_cast<double>(*reinterpret_cast<const std::int8_t*>(p));
        case 'B': return static_cast<double>(*reinterpret_cast<const std::uint8_t*>(p));
        case 'h': return static_cast<double>(*reinterpret_cast<const std::int16_t*>(p));
        case 'H': return static_cast<double>(*reinterpret_cast<const std::uint16_t*>(p));
        case 'i': case 'l':
            return static_cast<double>(*reinterpret_cast<const int*>(p));
        case 'I': case 'L':
            return static_cast<double>(*reinterpret_cast<const unsigned int*>(p));
        case 'q':
            return static_cast<double>(*reinterpret_cast<const long long*>(p));
        case 'Q':
            return static_cast<double>(*reinterpret_cast<const unsigned long long*>(p));
        default:
            return *reinterpret_cast<const double*>(p);
    }
}

std::vector<Point> to_points(py::object xo, py::object yo) {
    Numpy1D x = numpy_1d(xo);
    Numpy1D y = numpy_1d(yo);
    const py::ssize_t n = std::min(x.n, y.n);
    std::vector<Point> points;
    points.reserve(static_cast<std::size_t>(n));
    for (py::ssize_t i = 0; i < n; ++i) {
        points.push_back(Point{read_at(x, i), read_at(y, i)});
    }
    return points;
}

std::vector<double> to_doubles(py::object obj) {
    return obj.cast<std::vector<double>>();
}

std::unique_ptr<Chart> make_line(py::object x, py::object y, const std::string& title) {
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

std::unique_ptr<Chart> make_scatter(py::object x, py::object y, const std::string& title) {
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

std::unique_ptr<Chart> make_bar(const std::vector<std::string>& labels, py::object values,
                                const std::string& title) {
    auto chart = std::make_unique<termchart::chart::BarChart>();
    if (!title.empty()) {
        chart->set_title(title);
    }
    std::vector<double> vals = to_doubles(values);
    std::vector<std::string> names = labels;
    if (names.size() < vals.size()) {
        names.reserve(vals.size());
        for (std::size_t i = names.size(); i < vals.size(); ++i) {
            names.push_back(std::to_string(i));
        }
    }
    chart->set_values(vals, names,
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

std::string to_ansi(Chart& chart, std::uint32_t width, std::uint32_t height) {
    const termchart::api::ApiOptions opts = make_opts(width, height);
    const auto rendered = termchart::api::render_chart(chart, opts);
    if (!rendered.has_value()) {
        throw std::runtime_error("termchart: render_chart failed");
    }
    return rendered.value();
}

std::string to_svg(const Chart& chart, std::uint32_t width, std::uint32_t height) {
    const auto rendered = termchart::api::chart_to_svg(chart, width, height);
    if (!rendered.has_value()) {
        throw std::runtime_error("termchart: chart_to_svg failed (chart type not migrated to Scene IR)");
    }
    return rendered.value();
}

termchart::image::Bitmap render_bitmap(const Chart& chart, std::uint32_t width, std::uint32_t height,
                              std::uint32_t pixel_width, std::uint32_t pixel_height) {
    const auto scene = termchart::api::chart_to_scene(chart, width, height);
    if (!scene.has_value()) {
        throw std::runtime_error("termchart: chart_to_scene failed (chart type not migrated to Scene IR)");
    }
    termchart::gui::RenderOptions render_options;
    render_options.width = pixel_width;
    render_options.height = pixel_height;
    termchart::gui::SceneRenderer renderer(render_options);
    return renderer.render(*scene);
}

void to_png_file(const Chart& chart, const std::string& path, std::uint32_t width,
                 std::uint32_t height, std::uint32_t pixel_width, std::uint32_t pixel_height) {
    const termchart::image::Bitmap bitmap = render_bitmap(chart, width, height, pixel_width, pixel_height);
    const termchart::Status status =
        termchart::api::save_image(bitmap, path, termchart::image::ImageFormat::Png);
    if (status != termchart::ErrorCode::Ok) {
        throw std::runtime_error("termchart: save_png failed (encode or write error)");
    }
}

// Pixel buffer -> PNG bytes / file. Accepts a bytes object or a numpy uint8 array
// of size w*h*4 (RGBA); lets Python users hand back a surface they edited.
void save_png_pixels(py::object pixels, int w, int h, const std::string& path) {
    if (w <= 0 || h <= 0) {
        throw std::runtime_error("termchart: non-positive image size");
    }
    const std::size_t expected = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4;
    termchart::image::Bitmap bitmap(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), 4);
    if (py::isinstance<py::bytes>(pixels)) {
        std::string s = pixels.cast<std::string>();
        if (s.size() != expected) {
            throw std::runtime_error("termchart: pixel buffer size mismatch (need w*h*4 RGBA)");
        }
        std::memcpy(bitmap.data(), s.data(), expected);
    } else if (py::isinstance<py::array_t<std::uint8_t>>(pixels)) {
        auto arr = pixels.cast<py::array_t<std::uint8_t>>();
        py::buffer_info info = arr.request();
        if (static_cast<std::size_t>(info.size) != expected) {
            throw std::runtime_error("termchart: pixel buffer size mismatch (need w*h*4 RGBA)");
        }
        std::memcpy(bitmap.data(), info.ptr, expected);
    } else {
        throw std::runtime_error("termchart: save_png_pixels wants bytes or uint8 ndarray");
    }
    const termchart::Status status =
        termchart::api::save_image(bitmap, path, termchart::image::ImageFormat::Png);
    if (status != termchart::ErrorCode::Ok) {
        throw std::runtime_error("termchart: save_png failed (encode or write error)");
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

// Raw RGBA/RGB pixel accessors for embedding into other surfaces (Tkinter/PIL).
// The *_bytes variants need no numpy; the array variants return np.ndarray.
py::bytes to_rgba_bytes(py::object x, py::object y, const std::string& title, std::uint32_t width,
                        std::uint32_t height, std::uint32_t pixel_width,
                        std::uint32_t pixel_height) {
    auto chart = make_line(x, y, title);
    const termchart::image::Bitmap bitmap = render_bitmap(*chart, width, height, pixel_width, pixel_height);
    const std::size_t n = bitmap.size();
    return py::bytes(reinterpret_cast<const char*>(bitmap.data()), static_cast<py::ssize_t>(n));
}

py::bytes to_rgb_bytes(py::object x, py::object y, const std::string& title, std::uint32_t width,
                       std::uint32_t height, std::uint32_t pixel_width,
                       std::uint32_t pixel_height) {
    auto chart = make_line(x, y, title);
    const termchart::image::Bitmap bitmap = render_bitmap(*chart, width, height, pixel_width, pixel_height);
    const termchart::image::Bitmap rgb = bitmap.without_alpha();
    const std::size_t n = rgb.size();
    return py::bytes(reinterpret_cast<const char*>(rgb.data()), static_cast<py::ssize_t>(n));
}

py::array_t<std::uint8_t> to_rgba(py::object x, py::object y, const std::string& title,
                                  std::uint32_t width, std::uint32_t height,
                                  std::uint32_t pixel_width, std::uint32_t pixel_height) {
    auto chart = make_line(x, y, title);
    const termchart::image::Bitmap bitmap = render_bitmap(*chart, width, height, pixel_width, pixel_height);
    const std::uint32_t w = bitmap.width();
    const std::uint32_t h = bitmap.height();
    py::array_t<std::uint8_t, py::array::c_style> arr(
        {static_cast<py::ssize_t>(h), static_cast<py::ssize_t>(w), static_cast<py::ssize_t>(4)});
    std::memcpy(arr.mutable_data(), bitmap.data(), bitmap.size());
    return arr;
}

py::array_t<std::uint8_t> to_rgb(py::object x, py::object y, const std::string& title,
                                std::uint32_t width, std::uint32_t height,
                                std::uint32_t pixel_width, std::uint32_t pixel_height) {
    auto chart = make_line(x, y, title);
    const termchart::image::Bitmap bitmap = render_bitmap(*chart, width, height, pixel_width, pixel_height);
    const termchart::image::Bitmap rgb = bitmap.without_alpha();
    const std::uint32_t w = rgb.width();
    const std::uint32_t h = rgb.height();
    py::array_t<std::uint8_t, py::array::c_style> arr(
        {static_cast<py::ssize_t>(h), static_cast<py::ssize_t>(w), static_cast<py::ssize_t>(3)});
    std::memcpy(arr.mutable_data(), rgb.data(), rgb.size());
    return arr;
}

py::bytes to_png_bytes(py::object x, py::object y, const std::string& title, std::uint32_t width,
                       std::uint32_t height, std::uint32_t pixel_width,
                       std::uint32_t pixel_height) {
    auto chart = make_line(x, y, title);
    const termchart::image::Bitmap bitmap = render_bitmap(*chart, width, height, pixel_width, pixel_height);
    const std::string encoded = termchart::image::encode(bitmap, termchart::image::ImageFormat::Png);
    return py::bytes(encoded.data(), static_cast<py::ssize_t>(encoded.size()));
}

// pandas helpers (duck-typed: no pandas headers at build time). A column is
// fetched with df[col]; the row index with df.index; both expose .to_numpy().
std::unique_ptr<Chart> make_line_df(py::object df, const std::string& xcol,
                                   const std::string& ycol, const std::string& title) {
    auto col = [&](const std::string& c) -> py::object {
        if (c.empty()) {
            return df.attr("index").attr("to_numpy")();
        }
        return df.attr("__getitem__")(py::str(c));
    };
    return make_line(col(xcol), col(ycol), title);
}

std::unique_ptr<Chart> make_line_series(py::object series, const std::string& title) {
    py::object y = series.attr("to_numpy")();
    py::object x = series.attr("index").attr("to_numpy")();
    return make_line(x, y, title);
}

// ── pybind entry wrappers ───────────────────────────────────────────────

std::string py_plot(py::object x, py::object y, const std::string& title, std::uint32_t width,
                    std::uint32_t height) {
    auto chart = make_line(x, y, title);
    return to_ansi(*chart, width, height);
}

std::string py_scatter(py::object x, py::object y, const std::string& title, std::uint32_t width,
                       std::uint32_t height) {
    auto chart = make_scatter(x, y, title);
    return to_ansi(*chart, width, height);
}

std::string py_bar(const std::vector<std::string>& labels, py::object values,
                   const std::string& title, std::uint32_t width, std::uint32_t height) {
    auto chart = make_bar(labels, values, title);
    return to_ansi(*chart, width, height);
}

std::string py_plot_svg(py::object x, py::object y, const std::string& title, std::uint32_t width,
                        std::uint32_t height) {
    auto chart = make_line(x, y, title);
    return to_svg(*chart, width, height);
}

std::string py_scatter_svg(py::object x, py::object y, const std::string& title,
                           std::uint32_t width, std::uint32_t height) {
    auto chart = make_scatter(x, y, title);
    return to_svg(*chart, width, height);
}

std::string py_bar_svg(const std::vector<std::string>& labels, py::object values,
                       const std::string& title, std::uint32_t width, std::uint32_t height) {
    auto chart = make_bar(labels, values, title);
    return to_svg(*chart, width, height);
}

std::string py_plot_df(py::object df, const std::string& xcol, const std::string& ycol,
                       const std::string& title, std::uint32_t width, std::uint32_t height) {
    auto chart = make_line_df(df, xcol, ycol, title);
    return to_ansi(*chart, width, height);
}

std::string py_plot_series(py::object series, const std::string& title, std::uint32_t width,
                           std::uint32_t height) {
    auto chart = make_line_series(series, title);
    return to_ansi(*chart, width, height);
}

void py_save_svg(py::object x, py::object y, const std::string& path, const std::string& title,
                 std::uint32_t width, std::uint32_t height) {
    auto chart = make_line(x, y, title);
    write_text_file(path, to_svg(*chart, width, height));
}

void py_save_png_from_xy(py::object x, py::object y, const std::string& path,
                         const std::string& title, std::uint32_t width, std::uint32_t height,
                         std::uint32_t pixel_width, std::uint32_t pixel_height) {
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
    m.doc() = "termchart -- zero-dependency terminal charting (matplotlib-style one-liners)";

    m.attr("__version__") = TERMCHART_VERSION_STRING;

    m.def("version", []() { return std::string(TERMCHART_VERSION_STRING); },
          "Return the termchart version string.");

    m.def("plot", &py_plot, py::arg("x"), py::arg("y"), py::arg("title") = std::string{},
          py::arg("width") = kDefaultWidth, py::arg("height") = kDefaultHeight,
          "Draw a line chart and return the ANSI text (terminal cells). x/y accept a list, "
          "tuple, numpy array, or (column of) pandas Series.");
    m.def("scatter", &py_scatter, py::arg("x"), py::arg("y"), py::arg("title") = std::string{},
          py::arg("width") = kDefaultWidth, py::arg("height") = kDefaultHeight,
          "Draw a scatter chart and return the ANSI text.");
    m.def("bar", &py_bar, py::arg("labels"), py::arg("values"), py::arg("title") = std::string{},
          py::arg("width") = kDefaultWidth, py::arg("height") = kDefaultHeight,
          "Draw a bar chart and return the ANSI text. labels may be empty; values is a sequence.");

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

    m.def("save_png", &py_save_png_from_xy, py::arg("x"), py::arg("y"), py::arg("path"),
          py::arg("title") = std::string{}, py::arg("width") = kDefaultWidth,
          py::arg("height") = kDefaultHeight, py::arg("pixel_width") = kDefaultPixelWidth,
          py::arg("pixel_height") = kDefaultPixelHeight,
          "Draw a line chart and write a PNG file (chart -> Scene -> bitmap -> PNG)."
          );

    m.def("render_line", &py_render_line, py::arg("text"), py::arg("width") = 0u,
          "Render a piece of text as a styled ANSI terminal line.");

    // pandas-friendly entry points.
    m.def("plot_df", &py_plot_df, py::arg("df"), py::arg("x") = std::string{},
          py::arg("y") = std::string{}, py::arg("title") = std::string{},
          py::arg("width") = kDefaultWidth, py::arg("height") = kDefaultHeight,
          "Plot columns of a DataFrame (df[x], df[y]); omit x to use the index.");
    m.def("plot_series", &py_plot_series, py::arg("series"), py::arg("title") = std::string{},
          py::arg("width") = kDefaultWidth, py::arg("height") = kDefaultHeight,
          "Plot a pandas Series (index -> x, values -> y).");

    // Pixel buffers for embedding into other surfaces (Tkinter / PIL / Qt).
    m.def("to_rgba_bytes", &to_rgba_bytes, py::arg("x"), py::arg("y"),
          py::arg("title") = std::string{}, py::arg("width") = kDefaultWidth,
          py::arg("height") = kDefaultHeight, py::arg("pixel_width") = kDefaultPixelWidth,
          py::arg("pixel_height") = kDefaultPixelHeight,
          "Render and return the RGBA pixels as bytes (no numpy needed)."
          );
    m.def("to_rgb_bytes", &to_rgb_bytes, py::arg("x"), py::arg("y"),
          py::arg("title") = std::string{}, py::arg("width") = kDefaultWidth,
          py::arg("height") = kDefaultHeight, py::arg("pixel_width") = kDefaultPixelWidth,
          py::arg("pixel_height") = kDefaultPixelHeight,
          "Render and return the RGB pixels as bytes (no numpy needed)."
          );
    m.def("to_rgba", &to_rgba, py::arg("x"), py::arg("y"), py::arg("title") = std::string{},
          py::arg("width") = kDefaultWidth, py::arg("height") = kDefaultHeight,
          py::arg("pixel_width") = kDefaultPixelWidth, py::arg("pixel_height") = kDefaultPixelHeight,
          "Render and return the RGBA pixels as a numpy (H, W, 4) uint8 array."
          );
    m.def("to_rgb", &to_rgb, py::arg("x"), py::arg("y"), py::arg("title") = std::string{},
          py::arg("width") = kDefaultWidth, py::arg("height") = kDefaultHeight,
          py::arg("pixel_width") = kDefaultPixelWidth, py::arg("pixel_height") = kDefaultPixelHeight,
          "Render and return the RGB pixels as a numpy (H, W, 3) uint8 array."
          );
    m.def("to_png_bytes", &to_png_bytes, py::arg("x"), py::arg("y"), py::arg("title") = std::string{},
          py::arg("width") = kDefaultWidth, py::arg("height") = kDefaultHeight,
          py::arg("pixel_width") = kDefaultPixelWidth, py::arg("pixel_height") = kDefaultPixelHeight,
          "Render and return the PNG file bytes."
          );
    m.def("save_png_pixels", &save_png_pixels, py::arg("pixels"), py::arg("width"),
          py::arg("height"), py::arg("path"),
          "Encode a w*h*4 RGBA pixel buffer (bytes or uint8 ndarray) to a PNG file.");

#ifdef TERMCHART_HAVE_QT
    m.def("show_qt", [](py::object x, py::object y, const std::string& title, std::uint32_t width,
                        std::uint32_t height, std::uint32_t pixel_width,
                        std::uint32_t pixel_height) {
        auto chart = make_line(x, y, title);
        const termchart::image::Bitmap bitmap =
            render_bitmap(*chart, width, height, pixel_width, pixel_height);
        termchart_gui_qt::show_bitmap(bitmap, title);
    }, py::arg("x"), py::arg("y"), py::arg("title") = std::string{},
       py::arg("width") = kDefaultWidth, py::arg("height") = kDefaultHeight,
       py::arg("pixel_width") = kDefaultPixelWidth, py::arg("pixel_height") = kDefaultPixelHeight,
       "Open a Qt window showing the chart (requires a Qt-enabled build).");
#endif
}

#endif  // TERMCHART_HAVE_PYBIND11

