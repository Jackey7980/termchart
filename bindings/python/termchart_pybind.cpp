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
#include <cstdio>
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
#include "termchart/image/decoder.hpp"
#include "termchart/image/encode.hpp"
#include "termchart/util/file.hpp"
#include "termchart/version.hpp"

// 通用图表模型走 C ABI：tc_chart_create(kind) 覆盖全部已登记图种，新增图种
// 自动对 Python 可见，不必在这里按图种手写一遍包装（那正是"牵一发而动全身"）。
#include "termchart_capi.h"

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
    // 走 util::fopen_utf8：Windows 上 ofstream(窄字符串) 按 ANSI 解释路径，
    // 含中文的用户目录会直接打不开（save_svg 因此失败）。
    std::FILE* fp = termchart::util::fopen_utf8(path, "wb");
    if (fp == nullptr) {
        throw std::runtime_error("termchart: cannot open file: " + path);
    }
    const std::size_t written = std::fwrite(text.data(), 1, text.size(), fp);
    const bool ok = (written == text.size());
    std::fclose(fp);
    if (!ok) {
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

// ── C ABI helpers ────────────────────────────────────────────────────────

std::string take_error(tc_error_handle err) {
    if (err == nullptr) {
        return {};
    }
    char buf[512] = {0};
    tc_error_message(err, buf, sizeof(buf));
    tc_error_free(err);
    return std::string(buf);
}

void check_status(tc_status st, tc_error_handle err, const char* what) {
    if (st == TC_OK) {
        // 成功时 err 理论上不填；仍释放以杜绝泄漏。
        if (err != nullptr) {
            tc_error_free(err);
        }
        return;
    }
    const std::string msg = take_error(err);
    throw std::runtime_error(std::string("termchart: ") + what + " failed (status " +
                             std::to_string(st) + ")" + (msg.empty() ? "" : ": " + msg));
}

tc_color_depth depth_of(const std::string& name) {
    if (name == "none") return TC_DEPTH_NONE;
    if (name == "16" || name == "ansi16") return TC_DEPTH_16;
    if (name == "256" || name == "ansi256") return TC_DEPTH_256;
    return TC_DEPTH_TRUECOLOR;
}

termchart::image::ImageFormat format_of(const std::string& name) {
    std::string f = name;
    std::transform(f.begin(), f.end(), f.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (f == "png") return termchart::image::ImageFormat::Png;
    if (f == "jpg" || f == "jpeg") return termchart::image::ImageFormat::Jpeg;
    if (f == "gif") return termchart::image::ImageFormat::Gif;
    if (f == "webp") return termchart::image::ImageFormat::Webp;
    if (f == "mp4") return termchart::image::ImageFormat::Mp4;
    throw std::runtime_error("termchart: unsupported image format '" + name +
                             "' (png / jpeg / gif / webp / mp4)");
}

std::vector<std::string> strings_of(py::object labels) {
    std::vector<std::string> out;
    if (labels.is_none()) {
        return out;
    }
    out.reserve(static_cast<std::size_t>(py::len(labels)));
    for (auto item : py::iter(labels)) {
        out.push_back(py::cast<std::string>(item));
    }
    return out;
}

// 标签指针只在调用期间需要有效：把 string 留在调用方的容器里。
std::vector<const char*> c_strings(const std::vector<std::string>& values) {
    std::vector<const char*> out;
    out.reserve(values.size());
    for (const auto& s : values) {
        out.push_back(s.c_str());
    }
    return out;
}

std::vector<double> doubles_of(py::object obj) {
    if (obj.is_none()) {
        return {};
    }
    return to_doubles(obj);
}

py::array_t<std::uint8_t> bitmap_to_numpy(const termchart::image::Bitmap& bmp);

// ── 通用图表对象（全部图种，走 tc_chart_* 模型）──────────────────────────

class PyChart {
public:
    explicit PyChart(const std::string& kind) : kind_(kind) {
        tc_error_handle err = nullptr;
        handle_ = tc_chart_create(kind_.c_str(), &err);
        if (handle_ == nullptr) {
            const std::string msg = take_error(err);
            throw std::runtime_error("termchart: unknown chart kind '" + kind_ + "'" +
                                     (msg.empty() ? "" : ": " + msg) +
                                     " (see termchart.kinds() for the full list)");
        }
    }

    PyChart(const PyChart&) = delete;
    PyChart& operator=(const PyChart&) = delete;

    ~PyChart() {
        if (handle_ != nullptr) {
            tc_chart_free(handle_);
        }
    }

    [[nodiscard]] const std::string& kind() const noexcept { return kind_; }

    // ── 数据入口（与 C ABI 的五种形状一一对应）──

    void add_points(py::object x, py::object y) {
        std::vector<double> xs = doubles_of(x);
        std::vector<double> ys = doubles_of(y);
        const std::size_t n = std::min(xs.size(), ys.size());
        if (n == 0) {
            throw std::runtime_error("termchart: add_points needs non-empty x/y");
        }
        check_status(tc_chart_add_points(alive(), xs.data(), ys.data(), n), nullptr,
                     "add_points");
    }

    void add_series(py::object x, py::object y, const std::string& name,
                    py::object color) {
        add_many(py::make_tuple(py::make_tuple(x, y, name, color)));
    }

    void add_many(py::iterable series) {
        std::vector<std::vector<double>> xs_store;
        std::vector<std::vector<double>> ys_store;
        std::vector<std::string> names_store;
        std::vector<tc_series> items;

        for (auto item : series) {
            auto t = py::cast<py::tuple>(item);
            if (py::len(t) < 2) {
                throw std::runtime_error("termchart: series items need (x, y[, name[, color]])");
            }
            xs_store.push_back(to_doubles(t[0]));
            ys_store.push_back(to_doubles(t[1]));
            names_store.push_back(py::len(t) > 2 && !t[2].is_none()
                                      ? py::cast<std::string>(t[2])
                                      : std::string{});
            tc_series s{};
            s.name = names_store.back().empty() ? nullptr : names_store.back().c_str();
            if (py::len(t) > 3 && !t[3].is_none()) {
                const auto spec = py::cast<py::tuple>(t[3]);
                s.color.kind = TC_COLOR_RGB;
                s.color.r = py::cast<int>(spec[0]);
                s.color.g = py::cast<int>(spec[1]);
                s.color.b = py::cast<int>(spec[2]);
            }
            s.x = xs_store.back().data();
            s.y = ys_store.back().data();
            s.n = std::min(xs_store.back().size(), ys_store.back().size());
            if (s.n == 0) {
                throw std::runtime_error("termchart: series needs non-empty x/y");
            }
            items.push_back(s);
        }

        if (items.empty()) {
            return;
        }
        tc_error_handle err = nullptr;
        check_status(tc_chart_add_series(alive(), items.data(), items.size(), &err), err,
                     "add_series");
    }

    void set_values(py::object values, py::object labels) {
        std::vector<double> vals = doubles_of(values);
        std::vector<std::string> labs = strings_of(labels);
        std::vector<const char*> lab_ptrs = c_strings(labs);
        tc_error_handle err = nullptr;
        check_status(tc_chart_set_values(alive(), vals.data(), vals.size(),
                                         lab_ptrs.empty() ? nullptr : lab_ptrs.data(),
                                         lab_ptrs.size(), &err),
                     err, "set_values");
    }

    void set_matrix(py::object rows, py::object row_labels, py::object col_labels) {
        std::vector<std::vector<double>> rows_store;
        std::vector<const double*> row_ptrs;
        std::size_t ncol = 0;
        for (auto row : py::iter(rows)) {
            rows_store.push_back(to_doubles(py::reinterpret_borrow<py::object>(row)));
            if (ncol == 0) {
                ncol = rows_store.back().size();
            } else if (rows_store.back().size() != ncol) {
                throw std::runtime_error("termchart: set_matrix rows must all have the same length");
            }
        }
        if (rows_store.empty()) {
            throw std::runtime_error("termchart: set_matrix needs at least one row");
        }
        row_ptrs.reserve(rows_store.size());
        for (const auto& r : rows_store) {
            row_ptrs.push_back(r.data());
        }
        std::vector<std::string> rl = strings_of(row_labels);
        std::vector<std::string> cl = strings_of(col_labels);
        std::vector<const char*> rl_ptrs = c_strings(rl);
        std::vector<const char*> cl_ptrs = c_strings(cl);
        tc_error_handle err = nullptr;
        check_status(tc_chart_set_matrix(alive(), row_ptrs.data(), rows_store.size(), ncol,
                                         rl_ptrs.empty() ? nullptr : rl_ptrs.data(),
                                         rl_ptrs.size(),
                                         cl_ptrs.empty() ? nullptr : cl_ptrs.data(),
                                         cl_ptrs.size(), &err),
                     err, "set_matrix");
    }

    void set_scalar(double value) {
        tc_error_handle err = nullptr;
        check_status(tc_chart_set_scalar(alive(), value, &err), err, "set_scalar");
    }

    void set_tree(py::object nodes) {
        std::vector<int> parents;
        std::vector<double> values;
        std::vector<std::string> names;
        for (auto item : py::iter(nodes)) {
            auto t = py::cast<py::tuple>(item);
            if (py::len(t) != 3) {
                throw std::runtime_error("termchart: tree nodes are (parent_index, name, value)");
            }
            parents.push_back(py::cast<int>(t[0]));
            names.push_back(py::cast<std::string>(t[1]));
            values.push_back(py::cast<double>(t[2]));
        }
        if (parents.empty()) {
            throw std::runtime_error("termchart: set_tree needs at least one node");
        }
        std::vector<const char*> name_ptrs = c_strings(names);
        tc_error_handle err = nullptr;
        check_status(tc_chart_set_tree(alive(), parents.data(), name_ptrs.data(),
                                       values.data(), parents.size(), &err),
                     err, "set_tree");
    }

    void set_text(py::iterable rows, py::object blob) {
        std::vector<std::vector<std::string>> cells;
        std::vector<std::vector<const char*>> cell_ptrs;
        std::vector<const char* const*> row_ptrs;
        std::vector<std::uint64_t> counts;
        for (auto row : rows) {
            std::vector<std::string> line;
            for (auto cell : py::iter(row)) {
                line.push_back(py::cast<std::string>(cell));
            }
            cells.push_back(std::move(line));
        }
        for (const auto& line : cells) {
            cell_ptrs.push_back(c_strings(line));
            counts.push_back(line.size());
        }
        for (const auto& line : cell_ptrs) {
            row_ptrs.push_back(line.data());
        }
        std::string geo = blob.is_none() ? std::string{} : py::cast<std::string>(blob);
        tc_error_handle err = nullptr;
        check_status(tc_chart_set_text(alive(), row_ptrs.data(), row_ptrs.size(),
                                       counts.data(),
                                       geo.empty() ? nullptr : geo.c_str(), &err),
                     err, "set_text");
    }

    void set_regions(py::iterable regions) {
        std::vector<std::string> names;
        std::vector<double> values;
        std::vector<double> cell_x;
        std::vector<double> cell_y;
        std::vector<std::uint64_t> starts;
        std::vector<std::uint64_t> counts;
        for (auto item : py::iter(regions)) {
            auto t = py::cast<py::tuple>(item);
            if (py::len(t) < 2) {
                throw std::runtime_error("termchart: regions are (name, value[, cells])");
            }
            names.push_back(py::cast<std::string>(t[0]));
            values.push_back(py::cast<double>(t[1]));
            starts.push_back(cell_x.size());
            std::size_t added = 0;
            if (py::len(t) > 2 && !t[2].is_none()) {
                for (auto cell : py::iter(t[2])) {
                    auto xy = py::cast<py::tuple>(cell);
                    cell_x.push_back(py::cast<double>(xy[0]));
                    cell_y.push_back(py::cast<double>(xy[1]));
                    ++added;
                }
            }
            counts.push_back(added);
        }
        if (names.empty()) {
            throw std::runtime_error("termchart: set_regions needs at least one region");
        }
        std::vector<const char*> name_ptrs = c_strings(names);
        tc_error_handle err = nullptr;
        check_status(tc_chart_set_regions(alive(), name_ptrs.data(), values.data(),
                                          names.size(),
                                          cell_x.empty() ? nullptr : cell_x.data(),
                                          cell_y.empty() ? nullptr : cell_y.data(),
                                          starts.data(), counts.data(), cell_x.size(), &err),
                     err, "set_regions");
    }

    // ── 模型属性（一条通道，与 C ABI 的 tc_chart_set_prop 同构）──

    void set_prop(const std::string& prop, py::object value) {
        const tc_chart_prop id = prop_id(prop);
        tc_value v{};
        std::vector<double> nums;
        std::string str;
        if (value.is_none()) {
            v.kind = TC_VAL_NONE;
        } else if (prop == "auto_domain" || prop == "include_zero" ||
                   prop == "emphasize_zero" || prop == "shade_negative") {
            v.kind = TC_VAL_BOOL;
            v.i = py::cast<bool>(value) ? 1 : 0;
        } else if (prop == "x_label" || prop == "y_label") {
            str = py::cast<std::string>(value);
            v.kind = TC_VAL_STRING;
            v.s = str.c_str();
        } else {
            nums = to_doubles(value);
            v.kind = TC_VAL_DOUBLE_ARRAY;
            v.p = nums.data();
            v.n = nums.size();
        }
        tc_error_handle err = nullptr;
        check_status(tc_chart_set_prop(alive(), id, &v, &err), err,
                     ("set_prop(" + prop + ")").c_str());
    }

    // ── 输出 ──

    std::string ansi(std::optional<std::uint32_t> width, std::optional<std::uint32_t> height,
                     std::optional<std::string> title, std::optional<bool> show_legend,
                     const std::string& depth) {
        tc_plot_opts opts = plot_opts(width, height, title, show_legend, depth);
        char* out = nullptr;
        tc_error_handle err = nullptr;
        check_status(tc_chart_render_ansi(alive(), &opts, &out, &err), err, "render");
        const std::string text = out == nullptr ? std::string{} : std::string(out);
        tc_free_string(out);
        return text;
    }

    void print(std::optional<std::uint32_t> width, std::optional<std::uint32_t> height,
               std::optional<std::string> title, std::optional<bool> show_legend,
               const std::string& depth) {
        tc_plot_opts opts = plot_opts(width, height, title, show_legend, depth);
        tc_error_handle err = nullptr;
        check_status(tc_chart_print(alive(), &opts, &err), err, "print");
    }

    std::string to_svg(std::optional<std::uint32_t> width, std::optional<std::uint32_t> height,
                       std::optional<std::string> title, std::optional<bool> show_legend) {
        tc_plot_opts opts = plot_opts(width, height, title, show_legend, std::string{"truecolor"});
        return take_buffer(&tc_chart_to_svg, opts, "to_svg");
    }

    std::string to_png(std::optional<std::uint32_t> width, std::optional<std::uint32_t> height,
                       std::optional<std::string> title, std::optional<bool> show_legend) {
        tc_plot_opts opts = plot_opts(width, height, title, show_legend, std::string{"truecolor"});
        return take_buffer(&tc_chart_to_png, opts, "to_png");
    }

    // 像素数组：复用 PNG 导出 → 零依赖 PNG 解码，避免与 SVG/PNG 走出两套栅格化。
    py::array_t<std::uint8_t> pixels(std::optional<std::uint32_t> width,
                                     std::optional<std::uint32_t> height,
                                     std::optional<std::string> title,
                                     std::optional<bool> show_legend) {
        const std::string png = to_png(width, height, title, show_legend);
        const auto decoded = termchart::image::decode_image(
            reinterpret_cast<const std::uint8_t*>(png.data()), png.size());
        if (!decoded.has_value()) {
            throw std::runtime_error("termchart: pixels() failed to decode the rendered PNG");
        }
        return bitmap_to_numpy(decoded.value());
    }

private:
    [[nodiscard]] tc_chart_handle alive() const {
        if (handle_ == nullptr) {
            throw std::runtime_error("termchart: chart is already closed");
        }
        return handle_;
    }

public:
    // 供 PyDoc::add_chart 取出同一句柄；不复制图表，也不重新建模型。
    [[nodiscard]] tc_chart_handle raw() const { return alive(); }

private:
    static tc_chart_prop prop_id(const std::string& prop) {
        if (prop == "y_domain") return TC_PROP_Y_DOMAIN;
        if (prop == "auto_domain") return TC_PROP_AUTO_DOMAIN;
        if (prop == "include_zero") return TC_PROP_INCLUDE_ZERO;
        if (prop == "emphasize_zero") return TC_PROP_EMPHASIZE_ZERO;
        if (prop == "shade_negative") return TC_PROP_SHADE_NEGATIVE;
        if (prop == "x_ticks") return TC_PROP_X_TICKS;
        if (prop == "x_label") return TC_PROP_X_LABEL;
        if (prop == "y_label") return TC_PROP_Y_LABEL;
        throw std::runtime_error("termchart: unknown chart property '" + prop +
                                 "' (y_domain / auto_domain / include_zero / emphasize_zero / "
                                 "shade_negative / x_ticks / x_label / y_label)");
    }

    static tc_plot_opts plot_opts(std::optional<std::uint32_t> width,
                                  std::optional<std::uint32_t> height,
                                  const std::optional<std::string>& title,
                                  std::optional<bool> show_legend,
                                  const std::string& depth) {
        tc_plot_opts opts{};
        opts.width = width.value_or(0);
        opts.height = height.value_or(0);
        opts.title = title.has_value() ? title->c_str() : nullptr;
        opts.depth = depth_of(depth);
        opts.show_legend = (show_legend.value_or(false)) ? 1 : 0;
        return opts;
    }

    using BufferFn = tc_status (*)(tc_chart_handle, const tc_plot_opts*, char*, uint64_t,
                                   uint64_t*, tc_error_handle*);

    std::string take_buffer(BufferFn fn, tc_plot_opts& opts, const char* what) {
        uint64_t need = 0;
        tc_error_handle err = nullptr;
        check_status(fn(alive(), &opts, nullptr, 0, &need, &err), err, what);
        std::vector<char> buf(static_cast<std::size_t>(need) + 1, '\0');
        uint64_t got = 0;
        check_status(fn(alive(), &opts, buf.data(), buf.size(), &got, &err), err, what);
        return std::string(buf.data(), static_cast<std::size_t>(got));
    }

    tc_chart_handle handle_ = nullptr;
    std::string kind_;
};

// ── 图像（Bitmap + 编解码，走 image:: 模块）────────────────────────────────

py::array_t<std::uint8_t> bitmap_to_numpy(const termchart::image::Bitmap& bmp) {
    py::array_t<std::uint8_t, py::array::c_style> arr(
        {static_cast<py::ssize_t>(bmp.height()), static_cast<py::ssize_t>(bmp.width()),
         static_cast<py::ssize_t>(bmp.channels())});
    std::memcpy(arr.mutable_data(), bmp.data(), bmp.size());
    return arr;
}

termchart::image::Bitmap bitmap_from_numpy(py::object obj) {
    py::array arr = py::array::ensure(obj);
    if (!arr) {
        throw std::runtime_error("termchart: image wants a uint8 numpy array");
    }
    py::buffer_info info = arr.request();
    if (info.ndim != 3 || info.itemsize != 1) {
        throw std::runtime_error("termchart: image must be an (H, W, 3|4) uint8 array");
    }
    const std::uint32_t h = static_cast<std::uint32_t>(info.shape[0]);
    const std::uint32_t w = static_cast<std::uint32_t>(info.shape[1]);
    const std::uint8_t c = static_cast<std::uint8_t>(info.shape[2]);
    if (c != 3 && c != 4) {
        throw std::runtime_error("termchart: image must have 3 or 4 channels");
    }
    termchart::image::Bitmap bmp(w, h, c);
    if (info.strides[0] == static_cast<py::ssize_t>(w) * c &&
        info.strides[1] == static_cast<py::ssize_t>(c)) {
        std::memcpy(bmp.data(), info.ptr, bmp.size());
    } else {
        const char* base = static_cast<const char*>(info.ptr);
        for (std::uint32_t y = 0; y < h; ++y) {
            for (std::uint32_t x = 0; x < w; ++x) {
                for (std::uint8_t ch = 0; ch < c; ++ch) {
                    const char* p = base + info.strides[0] * y + info.strides[1] * x + ch;
                    bmp.set(x, y, ch, *reinterpret_cast<const std::uint8_t*>(p));
                }
            }
        }
    }
    return bmp;
}

std::string read_file_bytes(const std::string& path) {
    std::FILE* fp = termchart::util::fopen_utf8(path, "rb");
    if (fp == nullptr) {
        throw std::runtime_error("termchart: cannot open file: " + path);
    }
    std::string out;
    char chunk[8192];
    std::size_t n = 0;
    while ((n = std::fread(chunk, 1, sizeof(chunk), fp)) > 0) {
        out.append(chunk, n);
    }
    std::fclose(fp);
    return out;
}

void write_file_bytes(const std::string& path, const std::string& data) {
    std::FILE* fp = termchart::util::fopen_utf8(path, "wb");
    if (fp == nullptr) {
        throw std::runtime_error("termchart: cannot open file: " + path);
    }
    const std::size_t written = std::fwrite(data.data(), 1, data.size(), fp);
    std::fclose(fp);
    if (written != data.size()) {
        throw std::runtime_error("termchart: cannot write file: " + path);
    }
}

// ── pybind entry wrappers ───────────────────────────────────────────────

// 一行式出图直通 C ABI 的 tc_quick_*（= api::render_plot / render_scatter /
// render_bar）。不再自己拼 Chart：那样系列名、配色、图例默认值会和 C++ 版本
// 分叉——这正是 Python 与 C++ "看起来不一样"的来源。
std::string quick_render_xy(tc_quick_kind kind, py::object x, py::object y,
                            const std::string& title, std::uint32_t width,
                            std::uint32_t height) {
    const std::vector<double> xs = to_doubles(x);
    const std::vector<double> ys = to_doubles(y);
    const std::size_t n = std::min(xs.size(), ys.size());
    if (n == 0) {
        throw std::runtime_error("termchart: plot needs non-empty x/y");
    }
    tc_plot_opts opts{};
    opts.width = width;
    opts.height = height;
    opts.title = title.empty() ? nullptr : title.c_str();
    opts.depth = TC_DEPTH_TRUECOLOR;
    std::uint64_t need = 0;
    tc_error_handle err = nullptr;
    const tc_status probe =
        tc_quick_xy(kind, xs.data(), ys.data(), n, &opts, nullptr, 0, &need, &err);
    if (probe != TC_ERR_BUFFER_TOO_SMALL) {
        const std::string msg = take_error(err);
        throw std::runtime_error("termchart: quick render failed" +
                                 (msg.empty() ? std::string{} : ": " + msg));
    }
    std::vector<char> buf(static_cast<std::size_t>(need) + 1, '\0');
    std::uint64_t got = 0;
    err = nullptr;
    check_status(tc_quick_xy(kind, xs.data(), ys.data(), n, &opts, buf.data(), buf.size(), &got,
                             &err),
                 err, "quick render");
    return std::string(buf.data(), static_cast<std::size_t>(got));
}

std::string py_plot(py::object x, py::object y, const std::string& title, std::uint32_t width,
                    std::uint32_t height) {
    return quick_render_xy(TC_QUICK_LINE, x, y, title, width, height);
}

std::string py_scatter(py::object x, py::object y, const std::string& title, std::uint32_t width,
                       std::uint32_t height) {
    return quick_render_xy(TC_QUICK_SCATTER, x, y, title, width, height);
}

std::string py_bar(py::object values, py::object labels, const std::string& title,
                   std::uint32_t width, std::uint32_t height) {
    const std::vector<double> vals = to_doubles(values);
    const std::vector<std::string> cats = strings_of(labels);
    std::vector<const char*> cat_ptrs = c_strings(cats);
    if (vals.empty()) {
        throw std::runtime_error("termchart: bar needs non-empty values");
    }
    tc_plot_opts opts{};
    opts.width = width;
    opts.height = height;
    opts.title = title.empty() ? nullptr : title.c_str();
    opts.depth = TC_DEPTH_TRUECOLOR;
    std::uint64_t need = 0;
    tc_error_handle err = nullptr;
    const tc_status probe = tc_quick_bar(vals.data(), cat_ptrs.empty() ? nullptr : cat_ptrs.data(),
                                         vals.size(), &opts, nullptr, 0, &need, &err);
    if (probe != TC_ERR_BUFFER_TOO_SMALL) {
        const std::string msg = take_error(err);
        throw std::runtime_error("termchart: quick bar failed" +
                                 (msg.empty() ? std::string{} : ": " + msg));
    }
    std::vector<char> buf(static_cast<std::size_t>(need) + 1, '\0');
    std::uint64_t got = 0;
    err = nullptr;
    check_status(tc_quick_bar(vals.data(), cat_ptrs.empty() ? nullptr : cat_ptrs.data(),
                              vals.size(), &opts, buf.data(), buf.size(), &got, &err),
                 err, "quick bar");
    return std::string(buf.data(), static_cast<std::size_t>(got));
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

std::string py_bar_svg(py::object values, py::object labels, const std::string& title,
                       std::uint32_t width, std::uint32_t height) {
    auto chart = make_bar(strings_of(labels), values, title);
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

// ── 图像对象（tc_image_* 句柄；方法就是 image::Bitmap 的同名成员）────────────

class PyBitmap {
public:
    explicit PyBitmap(tc_image_handle handle) : handle_(handle) {}
    PyBitmap(const PyBitmap&) = delete;
    PyBitmap& operator=(const PyBitmap&) = delete;
    // 工厂函数与 scaled_to/with_alpha 都按值返回，句柄所有权随之转移。
    PyBitmap(PyBitmap&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    PyBitmap& operator=(PyBitmap&& other) noexcept {
        if (this != &other) {
            if (handle_ != nullptr) {
                tc_image_free(handle_);
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ~PyBitmap() {
        if (handle_ != nullptr) {
            tc_image_free(handle_);
        }
    }

    static PyBitmap create(std::uint32_t w, std::uint32_t h, std::uint8_t channels) {
        tc_error_handle err = nullptr;
        tc_image_handle handle = tc_image_create(w, h, channels, &err);
        if (handle == nullptr) {
            throw std::runtime_error("termchart: image create failed: " + take_error(err));
        }
        return PyBitmap(handle);
    }

    static PyBitmap load(const std::string& path) {
        tc_error_handle err = nullptr;
        tc_image_handle handle = tc_image_load(path.c_str(), &err);
        if (handle == nullptr) {
            throw std::runtime_error("termchart: image load failed: " + take_error(err));
        }
        return PyBitmap(handle);
    }

    static PyBitmap decode(py::bytes data) {
        const std::string raw = data;
        tc_error_handle err = nullptr;
        tc_image_handle handle = tc_image_decode(
            reinterpret_cast<const std::uint8_t*>(raw.data()), raw.size(), &err);
        if (handle == nullptr) {
            throw std::runtime_error("termchart: image decode failed: " + take_error(err));
        }
        return PyBitmap(handle);
    }

    [[nodiscard]] std::uint32_t width() const { return tc_image_width(alive()); }
    [[nodiscard]] std::uint32_t height() const { return tc_image_height(alive()); }
    [[nodiscard]] std::uint8_t channels() const { return tc_image_channels(alive()); }

    // 原始像素是二进制：返回 py::bytes，别让 pybind11 当成 UTF-8 字符串解码。
    py::bytes pixels() const { return py::bytes(pixels_raw()); }

    py::array_t<std::uint8_t> numpy() const {
        const std::string pix = pixels_raw();
        py::array_t<std::uint8_t, py::array::c_style> arr(
            {static_cast<py::ssize_t>(height()), static_cast<py::ssize_t>(width()),
             static_cast<py::ssize_t>(channels())});
        std::memcpy(arr.mutable_data(), pix.data(), pix.size());
        return arr;
    }

    PyBitmap scaled_to(std::uint32_t w, std::uint32_t h) const {
        tc_error_handle err = nullptr;
        tc_image_handle out = tc_image_scaled(alive(), w, h, &err);
        if (out == nullptr) {
            throw std::runtime_error("termchart: image scaled_to failed: " + take_error(err));
        }
        return PyBitmap(out);
    }

    PyBitmap with_alpha(std::uint8_t alpha) const {
        tc_error_handle err = nullptr;
        tc_image_handle out = tc_image_with_alpha(alive(), alpha, &err);
        if (out == nullptr) {
            throw std::runtime_error("termchart: image with_alpha failed: " + take_error(err));
        }
        return PyBitmap(out);
    }

    PyBitmap without_alpha() const {
        tc_error_handle err = nullptr;
        tc_image_handle out = tc_image_without_alpha(alive(), &err);
        if (out == nullptr) {
            throw std::runtime_error("termchart: image without_alpha failed: " + take_error(err));
        }
        return PyBitmap(out);
    }

    void fill(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) const {
        tc_image_fill(alive(), r, g, b, a);
    }

    void fill_rect(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h,
                   std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) const {
        tc_image_fill_rect(alive(), x, y, w, h, r, g, b, a);
    }

    void blit_over(const PyBitmap& src, std::int32_t dx, std::int32_t dy) const {
        tc_image_blit_over(alive(), src.handle_, dx, dy);
    }

    py::bytes encode(const std::string& format, std::uint8_t quality) const {
        const std::uint64_t need = encode_size(format, quality);
        std::vector<char> buf(static_cast<std::size_t>(need));
        std::uint64_t got = 0;
        tc_error_handle err = nullptr;
        check_status(tc_image_encode(alive(), format.c_str(), quality, buf.data(), buf.size(),
                                     &got, &err),
                     err, "image encode");
        return py::bytes(buf.data(), static_cast<std::size_t>(got));
    }

    void save(const std::string& path, const std::string& format,
              std::uint8_t quality) const {
        tc_error_handle err = nullptr;
        check_status(
            tc_image_save(alive(), path.c_str(), format.empty() ? nullptr : format.c_str(),
                          quality, &err),
            err, "image save");
    }

private:
    [[nodiscard]] tc_image_handle alive() const {
        if (handle_ == nullptr) {
            throw std::runtime_error("termchart: image is already closed");
        }
        return handle_;
    }

    std::string pixels_raw() const {
        std::uint64_t need = 0;
        const tc_status probe = tc_image_pixels(alive(), nullptr, 0, &need, nullptr);
        if (probe != TC_ERR_BUFFER_TOO_SMALL) {
            throw std::runtime_error("termchart: image pixels failed");
        }
        std::vector<std::uint8_t> buf(static_cast<std::size_t>(need));
        std::uint64_t got = 0;
        tc_error_handle err = nullptr;
        check_status(tc_image_pixels(alive(), buf.data(), buf.size(), &got, &err), err,
                     "image pixels");
        return std::string(reinterpret_cast<const char*>(buf.data()),
                           static_cast<std::size_t>(got));
    }

    std::uint64_t encode_size(const std::string& format, std::uint8_t quality) const {
        std::uint64_t need = 0;
        tc_error_handle err = nullptr;
        const tc_status probe =
            tc_image_encode(alive(), format.c_str(), quality, nullptr, 0, &need, &err);
        if (probe != TC_ERR_BUFFER_TOO_SMALL) {
            throw std::runtime_error("termchart: image encode failed: " + take_error(err));
        }
        return need;
    }

    std::uint64_t encode_size(const std::string& format, std::uint8_t quality) const {
        std::uint64_t need = 0;
        tc_error_handle err = nullptr;
        const tc_status probe =
            tc_image_encode(alive(), format.c_str(), quality, nullptr, 0, &need, &err);
        if (probe != TC_ERR_BUFFER_TOO_SMALL) {
            throw std::runtime_error("termchart: image encode failed: " + take_error(err));
        }
        return need;
    }

    // 位图直接编码成终端协议字节流（sixel / kitty / iterm2 / 半块），
    // 与 C++ protocol:: 的四个编码器逐字节相同。
    py::bytes to_protocol(const std::string& kind, std::uint32_t cell_width,
                          std::uint32_t cell_height, bool rle) const {
        const tc_protocol_kind pk = protocol_kind(kind);
        const tc_protocol_opts po = protocol_opts(cell_width, cell_height, rle);
        uint64_t need = 0;
        tc_error_handle err = nullptr;
        check_status(tc_image_to_protocol(alive(), pk, &po, nullptr, 0, &need, &err), err,
                     "image to_protocol");
        std::vector<char> buf(static_cast<std::size_t>(need) + 1, '\0');
        uint64_t got = 0;
        check_status(tc_image_to_protocol(alive(), pk, &po, buf.data(), buf.size(), &got, &err),
                     err, "image to_protocol");
        return py::bytes(buf.data(), static_cast<std::size_t>(got));
    }

    tc_image_handle handle_ = nullptr;
};

// ── 关系图：一个句柄 + 属性通道（12 个图种共用）─────────────────────────────

tc_graph_prop graph_prop_id(const std::string& prop) {
    if (prop == "shape") return TC_GRAPH_PROP_SHAPE;
    if (prop == "route") return TC_GRAPH_PROP_ROUTE;
    if (prop == "edge_labels") return TC_GRAPH_PROP_EDGE_LABELS;
    if (prop == "arrows") return TC_GRAPH_PROP_ARROWS;
    if (prop == "node_border") return TC_GRAPH_PROP_NODE_BORDER;
    if (prop == "min_width") return TC_GRAPH_PROP_MIN_WIDTH;
    if (prop == "min_height") return TC_GRAPH_PROP_MIN_HEIGHT;
    if (prop == "h_gap") return TC_GRAPH_PROP_H_GAP;
    if (prop == "v_gap") return TC_GRAPH_PROP_V_GAP;
    if (prop == "mark_cycles") return TC_GRAPH_PROP_MARK_CYCLES;
    throw std::runtime_error("termchart: unknown graph property '" + prop +
                             "' (shape / route / edge_labels / arrows / node_border / "
                             "min_width / min_height / h_gap / v_gap / mark_cycles)");
}

// 把 Python 值包成 tc_value：数值走 INT/DOUBLE，布尔走 BOOL，None = 清除。
tc_value graph_value(py::object value) {
    tc_value v{};
    if (value.is_none()) {
        v.kind = TC_VAL_NONE;
        return v;
    }
    if (py::isinstance<py::bool_>(value)) {
        v.kind = TC_VAL_BOOL;
        v.i = py::cast<bool>(value) ? 1 : 0;
        return v;
    }
    if (py::isinstance<py::int_>(value)) {
        v.kind = TC_VAL_INT;
        v.i = py::cast<std::int64_t>(value);
        return v;
    }
    if (py::isinstance<py::float_>(value)) {
        v.kind = TC_VAL_DOUBLE;
        v.d = py::cast<double>(value);
        return v;
    }
    v.kind = TC_VAL_STRING;
    v.s = nullptr;  // 字符串属性不走 graph 通道（属性袋走 set_node_attr）
    throw std::runtime_error(
        "termchart: graph property accepts int / float / bool / None only");
}

tc_style rgb_style(py::object rgb, bool* ok) {
    tc_style st{};
    *ok = false;
    if (rgb.is_none()) return st;
    if (!py::isinstance<py::tuple>(rgb) && !py::isinstance<py::list>(rgb)) {
        throw std::runtime_error("termchart: color must be an (r, g, b) tuple");
    }
    const std::vector<int> v = py::cast<std::vector<int>>(rgb);
    if (v.size() != 3) {
        throw std::runtime_error("termchart: color must be an (r, g, b) tuple");
    }
    st.fg.kind = TC_COLOR_RGB;
    st.fg.r = static_cast<std::uint8_t>(v[0]);
    st.fg.g = static_cast<std::uint8_t>(v[1]);
    st.fg.b = static_cast<std::uint8_t>(v[2]);
    *ok = true;
    return st;
}

class PyGraph {
public:
    explicit PyGraph(const std::string& kind) {
        tc_error_handle err = nullptr;
        handle_ = tc_graph_create(kind.c_str(), &err);
        if (handle_ == nullptr) {
            throw std::runtime_error("termchart: graph create failed: " + take_error(err));
        }
    }

    static PyGraph from_text(const std::string& text, const std::string& format,
                             const std::string& kind) {
        tc_error_handle err = nullptr;
        tc_graph_handle h =
            tc_graph_from_text(text.c_str(), format.c_str(), kind.c_str(), &err);
        if (h == nullptr) {
            throw std::runtime_error("termchart: graph parse failed: " + take_error(err));
        }
        return PyGraph(h);
    }

    PyGraph(const PyGraph&) = delete;
    PyGraph& operator=(const PyGraph&) = delete;
    PyGraph(PyGraph&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    PyGraph& operator=(PyGraph&& other) noexcept {
        if (this != &other) {
            if (handle_ != nullptr) tc_graph_free(handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ~PyGraph() {
        if (handle_ != nullptr) tc_graph_free(handle_);
    }

    std::uint64_t add_node(const std::string& label) const {
        const std::uint64_t id = tc_graph_add_node(alive(), label.c_str());
        if (id == 0) throw std::runtime_error("termchart: graph add_node failed");
        return id;
    }

    void add_edge(std::uint64_t from, std::uint64_t to, const std::string& label) const {
        tc_error_handle err = nullptr;
        check_status(tc_graph_add_edge(alive(), from, to, label.empty() ? nullptr : label.c_str(),
                                       &err),
                     err, "graph add_edge");
    }

    void set_node_attr(std::uint64_t node, const std::string& key, const std::string& value) const {
        tc_error_handle err = nullptr;
        check_status(tc_graph_set_node_attr(alive(), node, key.c_str(), value.c_str(), &err), err,
                     "graph set_node_attr");
    }

    void set_edge_attr(std::uint64_t from, std::uint64_t to, const std::string& key,
                       const std::string& value) const {
        tc_error_handle err = nullptr;
        check_status(tc_graph_set_edge_attr(alive(), from, to, key.c_str(), value.c_str(), &err),
                     err, "graph set_edge_attr");
    }

    void set_prop(const std::string& prop, py::object value) const {
        tc_value v = graph_value(std::move(value));
        tc_error_handle err = nullptr;
        check_status(tc_graph_set_prop(alive(), graph_prop_id(prop), &v, &err), err,
                     ("set_prop(" + prop + ")").c_str());
    }

    void set_style(py::object node_color, py::object edge_color) const {
        bool has_node = false;
        bool has_edge = false;
        tc_style ns = rgb_style(std::move(node_color), &has_node);
        tc_style es = rgb_style(std::move(edge_color), &has_edge);
        tc_error_handle err = nullptr;
        check_status(tc_graph_set_style(alive(), has_node ? &ns : nullptr,
                                        has_edge ? &es : nullptr, &err),
                     err, "graph set_style");
    }

    [[nodiscard]] std::uint64_t node_count() const { return tc_graph_node_count(alive()); }
    [[nodiscard]] std::uint64_t edge_count() const { return tc_graph_edge_count(alive()); }

    std::string ansi(std::optional<std::uint32_t> width, std::optional<std::uint32_t> height,
                     const std::string& depth) const {
        tc_plot_opts opts = plot_opts_of(width, height, depth);
        uint64_t need = 0;
        tc_error_handle err = nullptr;
        check_status(tc_graph_render_ansi(alive(), &opts, nullptr, 0, &need, &err), err,
                     "graph render");
        std::vector<char> buf(static_cast<std::size_t>(need) + 1, '\0');
        uint64_t got = 0;
        check_status(tc_graph_render_ansi(alive(), &opts, buf.data(), buf.size(), &got, &err),
                     err, "graph render");
        return std::string(buf.data(), static_cast<std::size_t>(got));
    }

    void print(std::optional<std::uint32_t> width, std::optional<std::uint32_t> height,
               const std::string& depth) const {
        tc_plot_opts opts = plot_opts_of(width, height, depth);
        tc_error_handle err = nullptr;
        check_status(tc_graph_print(alive(), &opts, &err), err, "graph print");
    }

    py::bytes to_png(std::optional<std::uint32_t> width, std::optional<std::uint32_t> height) const {
        tc_plot_opts opts = plot_opts_of(width, height, std::string{"truecolor"});
        return py::bytes(buffer_of([&](char* buf, uint64_t cap, uint64_t* got) {
            tc_error_handle err = nullptr;
            return tc_graph_to_png(alive(), &opts, buf, cap, got, &err);
        }, "graph to_png"));
    }

    py::bytes to_protocol(const std::string& kind, std::optional<std::uint32_t> width,
                          std::optional<std::uint32_t> height, std::uint32_t cell_width,
                          std::uint32_t cell_height, bool rle) const {
        tc_plot_opts opts = plot_opts_of(width, height, std::string{"truecolor"});
        tc_protocol_opts po = protocol_opts(cell_width, cell_height, rle);
        return py::bytes(buffer_of([&](char* buf, uint64_t cap, uint64_t* got) {
            tc_error_handle err = nullptr;
            return tc_graph_to_protocol(alive(), protocol_kind(kind), &opts, &po, buf, cap, got,
                                        &err);
        }, "graph to_protocol"));
    }

private:
    explicit PyGraph(tc_graph_handle handle) : handle_(handle) {}

    [[nodiscard]] tc_graph_handle alive() const {
        if (handle_ == nullptr) throw std::runtime_error("termchart: graph is already closed");
        return handle_;
    }

    static tc_plot_opts plot_opts_of(std::optional<std::uint32_t> width,
                                     std::optional<std::uint32_t> height,
                                     const std::string& depth) {
        tc_plot_opts opts{};
        opts.width = width.value_or(0);
        opts.height = height.value_or(0);
        opts.depth = depth_of(depth);
        return opts;
    }

    template <typename Fn>
    static std::string buffer_of(Fn&& fn, const char* what) {
        uint64_t need = 0;
        tc_error_handle err = nullptr;
        check_status(fn(nullptr, 0, &need), err, what);
        std::vector<char> buf(static_cast<std::size_t>(need) + 1, '\0');
        uint64_t got = 0;
        check_status(fn(buf.data(), buf.size(), &got), err, what);
        return std::string(buf.data(), static_cast<std::size_t>(got));
    }

    tc_graph_handle handle_ = nullptr;
};

// ── 文档：块序列 → html / markdown / plain ─────────────────────────────────

class PyDoc {
public:
    PyDoc() {
        tc_error_handle err = nullptr;
        handle_ = tc_doc_create(&err);
        if (handle_ == nullptr) {
            throw std::runtime_error("termchart: document create failed: " + take_error(err));
        }
    }

    PyDoc(const PyDoc&) = delete;
    PyDoc& operator=(const PyDoc&) = delete;
    ~PyDoc() {
        if (handle_ != nullptr) tc_doc_free(handle_);
    }

    void add_heading(const std::string& text, std::uint32_t level) const {
        tc_error_handle err = nullptr;
        check_status(tc_doc_add_heading(alive(), text.c_str(), level, &err), err,
                     "doc add_heading");
    }
    void add_paragraph(const std::string& text) const {
        tc_error_handle err = nullptr;
        check_status(tc_doc_add_paragraph(alive(), text.c_str(), &err), err, "doc add_paragraph");
    }
    void add_code(const std::string& text, const std::string& language) const {
        tc_error_handle err = nullptr;
        check_status(tc_doc_add_code(alive(), text.c_str(),
                                     language.empty() ? nullptr : language.c_str(), &err),
                     err, "doc add_code");
    }

    void add_table(py::object header, py::object rows) const {
        // 单元指针只在调用期间有效：把字符串留在 vector 里，再取指针。
        std::vector<std::string> header_strings;
        std::vector<const char*> header_ptrs;
        if (!header.is_none()) {
            header_strings = strings_of(header);
            header_ptrs = c_strings(header_strings);
        }

        std::vector<std::vector<std::string>> cell_strings;
        std::vector<std::vector<const char*>> cell_ptrs;
        std::vector<const char* const*> row_ptrs;
        if (!rows.is_none()) {
            for (auto row : py::iter(rows)) {
                cell_strings.push_back(strings_of(row));
                cell_ptrs.push_back(c_strings(cell_strings.back()));
            }
            row_ptrs.reserve(cell_ptrs.size());
            for (const auto& r : cell_ptrs) row_ptrs.push_back(r.data());
        }

        const std::uint64_t n_cols =
            header.is_none() ? (cell_strings.empty() ? 0 : cell_strings.front().size())
                             : header_strings.size();
        tc_error_handle err = nullptr;
        check_status(tc_doc_add_table(alive(), header.is_none() ? nullptr : header_ptrs.data(),
                                      n_cols, row_ptrs.empty() ? nullptr : row_ptrs.data(),
                                      static_cast<std::uint64_t>(row_ptrs.size()), &err),
                     err, "doc add_table");
    }

    void add_chart(PyChart& chart, std::uint32_t width, std::uint32_t height) const {
        tc_error_handle err = nullptr;
        check_status(tc_doc_add_chart(alive(), chart.raw(), width, height, &err), err,
                     "doc add_chart");
    }

    std::string export_text(const std::string& format, const std::string& title,
                            const std::string& css_class) const {
        uint64_t need = 0;
        tc_error_handle err = nullptr;
        check_status(tc_doc_export(alive(), format.c_str(), title.empty() ? nullptr : title.c_str(),
                                   css_class.empty() ? nullptr : css_class.c_str(), nullptr, 0,
                                   &need, &err),
                     err, "doc export");
        std::vector<char> buf(static_cast<std::size_t>(need) + 1, '\0');
        uint64_t got = 0;
        check_status(tc_doc_export(alive(), format.c_str(), title.empty() ? nullptr : title.c_str(),
                                   css_class.empty() ? nullptr : css_class.c_str(), buf.data(),
                                   buf.size(), &got, &err),
                     err, "doc export");
        return std::string(buf.data(), static_cast<std::size_t>(got));
    }

    void save(const std::string& path, const std::string& format, const std::string& title,
              const std::string& css_class) const {
        tc_error_handle err = nullptr;
        check_status(tc_doc_save(alive(), path.c_str(), format.c_str(),
                                 title.empty() ? nullptr : title.c_str(),
                                 css_class.empty() ? nullptr : css_class.c_str(), &err),
                     err, "doc save");
    }

private:
    [[nodiscard]] tc_doc_handle alive() const {
        if (handle_ == nullptr) throw std::runtime_error("termchart: document is already closed");
        return handle_;
    }

    tc_doc_handle handle_ = nullptr;
};

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
    m.def("bar", &py_bar, py::arg("values"), py::arg("labels") = py::none(),
          py::arg("title") = std::string{}, py::arg("width") = kDefaultWidth,
          py::arg("height") = kDefaultHeight,
          "Draw a bar chart and return the ANSI text. labels may be omitted; values is a sequence.");

    m.def("plot_svg", &py_plot_svg, py::arg("x"), py::arg("y"), py::arg("title") = std::string{},
          py::arg("width") = kDefaultWidth, py::arg("height") = kDefaultHeight,
          "Draw a line chart and return the SVG text.");
    m.def("scatter_svg", &py_scatter_svg, py::arg("x"), py::arg("y"),
          py::arg("title") = std::string{}, py::arg("width") = kDefaultWidth,
          py::arg("height") = kDefaultHeight, "Draw a scatter chart and return the SVG text.");
    m.def("bar_svg", &py_bar_svg, py::arg("values"), py::arg("labels") = py::none(),
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

    // ── 通用图表模型：全部已登记图种，一个入口 ──
    py::class_<PyChart>(m, "Chart")
        .def(py::init<const std::string&>(), py::arg("kind"))
        .def_property_readonly("kind", &PyChart::kind)
        .def("add_points", &PyChart::add_points, py::arg("x"), py::arg("y"))
        .def("add_series", &PyChart::add_series, py::arg("x"), py::arg("y"),
             py::arg("name") = std::string{}, py::arg("color") = py::none())
        .def("add_many", &PyChart::add_many, py::arg("series"))
        .def("set_values", &PyChart::set_values, py::arg("values"),
             py::arg("labels") = py::none())
        .def("set_matrix", &PyChart::set_matrix, py::arg("rows"),
             py::arg("row_labels") = py::none(), py::arg("col_labels") = py::none())
        .def("set_scalar", &PyChart::set_scalar, py::arg("value"))
        .def("set_tree", &PyChart::set_tree, py::arg("nodes"))
        .def("set_text", &PyChart::set_text, py::arg("rows"), py::arg("blob") = py::none())
        .def("set_regions", &PyChart::set_regions, py::arg("regions"))
        .def("set_prop", &PyChart::set_prop, py::arg("prop"), py::arg("value"))
        .def("ansi", &PyChart::ansi, py::arg("width") = py::none(),
             py::arg("height") = py::none(), py::arg("title") = py::none(),
             py::arg("show_legend") = py::none(), py::arg("depth") = std::string{"truecolor"})
        .def("print", &PyChart::print, py::arg("width") = py::none(),
             py::arg("height") = py::none(), py::arg("title") = py::none(),
             py::arg("show_legend") = py::none(), py::arg("depth") = std::string{"truecolor"})
        .def("to_svg", &PyChart::to_svg, py::arg("width") = py::none(),
             py::arg("height") = py::none(), py::arg("title") = py::none(),
             py::arg("show_legend") = py::none())
        // PNG 是二进制：std::string 默认会被 pybind11 转成 str，遇到 0x89 就炸，
        // 所以这里显式转 py::bytes（与 ctypes 桥返回的 bytes 对齐）。
        .def("to_png", [](const PyChart& chart, std::optional<std::uint32_t> width,
                          std::optional<std::uint32_t> height,
                          std::optional<std::string> title, std::optional<bool> show_legend) {
            // 方法非 const（内部要 alive() 校验），这里用 const_cast 避免改签名。
            return py::bytes(const_cast<PyChart&>(chart).to_png(width, height, title, show_legend));
        }, py::arg("width") = py::none(), py::arg("height") = py::none(),
           py::arg("title") = py::none(), py::arg("show_legend") = py::none())
        .def("pixels", &PyChart::pixels, py::arg("width") = py::none(),
             py::arg("height") = py::none(), py::arg("title") = py::none(),
             py::arg("show_legend") = py::none())
        .def("__repr__", [](const PyChart& c) { return "<termchart.Chart '" + c.kind() + "'>"; });

    m.def("kinds", []() {
        std::uint64_t need = 0;
        tc_error_handle err = nullptr;
        // buf=nullptr 时返回 TC_OK 并填 out_len，不是 BUFFER_TOO_SMALL。
        const tc_status probe = tc_chart_kinds(nullptr, 0, &need, &err);
        if (probe != TC_OK && probe != TC_ERR_BUFFER_TOO_SMALL) {
            throw std::runtime_error("termchart: kinds failed: " + take_error(err));
        }
        std::vector<char> buf(static_cast<std::size_t>(need) + 1, '\0');
        std::uint64_t got = 0;
        check_status(tc_chart_kinds(buf.data(), buf.size(), &got, &err), err, "kinds");
        // tc_chart_kinds 用空格分隔（字典序）；按空白切分，别假设换行。
        std::vector<std::string> out;
        std::string current;
        for (std::uint64_t i = 0; i < got; ++i) {
            const char c = buf[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\0') {
                if (!current.empty()) {
                    out.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(c);
            }
        }
        if (!current.empty()) {
            out.push_back(current);
        }
        return out;
    }, "All registered chart kinds (use with Chart(kind)).");

    m.def("auto_plot", [](const std::string& path) {
        tc_error_handle err = nullptr;
        check_status(tc_auto_plot(path.c_str(), &err), err, "auto_plot");
    }, py::arg("path"), "Pick a chart automatically from a data file (CSV/JSON) and print it.");

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

    // ── image 子模块：与 image::Bitmap 同名成员，像素不重算 ──
    auto img = m.def_submodule("image", "Image decode/encode and pixel composition.");
    py::class_<PyBitmap>(img, "Bitmap")
        .def_static("create", &PyBitmap::create, py::arg("width"), py::arg("height"),
                    py::arg("channels") = 4)
        .def_static("load", &PyBitmap::load, py::arg("path"))
        .def_static("decode", &PyBitmap::decode, py::arg("data"))
        .def_property_readonly("width", &PyBitmap::width)
        .def_property_readonly("height", &PyBitmap::height)
        .def_property_readonly("channels", &PyBitmap::channels)
        .def("pixels", &PyBitmap::pixels)
        .def("numpy", &PyBitmap::numpy)
        .def("scaled_to", &PyBitmap::scaled_to, py::arg("width"), py::arg("height"))
        .def("with_alpha", &PyBitmap::with_alpha, py::arg("alpha") = 255)
        .def("without_alpha", &PyBitmap::without_alpha)
        .def("fill", &PyBitmap::fill, py::arg("r"), py::arg("g"), py::arg("b"),
             py::arg("a") = 255)
        .def("fill_rect", &PyBitmap::fill_rect, py::arg("x"), py::arg("y"), py::arg("w"),
             py::arg("h"), py::arg("r"), py::arg("g"), py::arg("b"), py::arg("a") = 255)
        .def("blit_over", &PyBitmap::blit_over, py::arg("src"), py::arg("dx"), py::arg("dy"))
        .def("encode", &PyBitmap::encode, py::arg("format") = std::string{"png"},
             py::arg("quality") = 85)
        .def("save", &PyBitmap::save, py::arg("path"), py::arg("format") = std::string{},
             py::arg("quality") = 85)
        .def("__repr__", [](const PyBitmap& b) {
            return "<termchart.image.Bitmap " + std::to_string(b.width()) + "x" +
                   std::to_string(b.height()) + " ch=" + std::to_string(b.channels()) + ">";
        });
    img.def("load", &PyBitmap::load, py::arg("path"), "Decode an image file.");
    img.def("decode", &PyBitmap::decode, py::arg("data"), "Decode image bytes.");

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

