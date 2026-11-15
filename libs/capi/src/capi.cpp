// libs/capi/src/capi.cpp —— termchart_capi 实现（C ABI，包裹 api:: 便捷层）
//
// 设计：唯一跨 C 边界的失败通道是 tc_error_handle + tc_status；绝不抛 C++ 异常。
// 字符串由本 TU 用 new[] 分配、调用方用 tc_free_string 释放（同 CRT，安全）。
//
// 覆盖面与 C++ 便捷入口 api:: 等价：
//   文本：render_line / print_line / hello（Style + ApiOptions）
//   图表：折线 / 散点 / 柱状 → ANSI 字节流，或直接写 stdout
//   输出：write_stdout / write_stderr / flush_stdout
//   自动：auto_plot（AutoViz）
#include "termchart_capi.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "termchart/api/autoviz_plot.hpp"
#include "termchart/api/chart.hpp"
#include "termchart/api/quick.hpp"
#include "termchart/backend/vector/backend.hpp"
#include "termchart/backend/vector/svg_backend.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/bar_chart.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/chart/cartesian/scatter_chart.hpp"
#include "termchart/chart/cartesian/sparkline.hpp"
#include "termchart/chart/context.hpp"
#include "termchart/chart/polar/pie.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/core/color_depth.hpp"
#include "termchart/core/error.hpp"
#include "termchart/core/style.hpp"
#include "termchart/gui/cells.hpp"
#include "termchart/image/encode.hpp"
#include "termchart/render/compositor.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"
#include "termchart/version.hpp"

using namespace termchart;

namespace {

// 句柄背后只有这一种数据：模型（系列 / 柱 / 属性）+ 惰性构建的图表对象。
//
// 惰性 + 缓存是**性能**的关键：同一张图先 render_ansi 再 print 只构建一次；
// 任何模型改动都经 invalidate() 丢弃缓存，不存在"改了没生效"。
struct ChartData {
    enum class Kind { Line, Scatter, Bar, Pie, Sparkline } kind = Kind::Line;

    // 折线 / 散点：多系列，每系列自带名字与颜色
    std::vector<chart::Series> series;

    // 柱状：单系列模型（一组类别 + 一组值）
    std::vector<double> values;
    std::vector<std::string> categories;
    std::string bar_name = "series";
    Color bar_color = chart::default_color(chart::DefaultColor::Accent);

    // 模型属性（展示属性在 tc_plot_opts 里按次传入，这里不重复存）
    std::optional<std::pair<double, double>> y_domain;
    bool auto_domain = true;
    bool include_zero = false;
    bool emphasize_zero = false;
    bool shade_negative = false;
    bool legend = false;  // 展示属性，按次渲染时设置；饼图在构建期据此设 PieOptions
    std::vector<double> x_ticks;

    // 轴标签：纯 C-ABI 层语义，不在 C++ 核心 Chart 上。渲染时后置绘制到边距里。
    std::string x_label;
    std::string y_label;

    std::unique_ptr<chart::Chart> cached;
    void invalidate() noexcept { cached.reset(); }
};

// 属性通道的两个小工具：bool 取值（BOOL/INT 都收）、"清除"判定（NONE 或空数组）。
inline bool bool_of(const tc_value& v, bool& out) {
    if (v.kind != TC_VAL_BOOL && v.kind != TC_VAL_INT) return false;
    out = v.i != 0;
    return true;
}

inline bool is_clear(const tc_value& v) {
    return v.kind == TC_VAL_NONE || (v.kind == TC_VAL_DOUBLE_ARRAY && (v.n == 0 || !v.p));
}

// ── 类型转换：C POD → C++ 值对象 ─────────────────────────────────────────────

inline ColorDepth to_depth(tc_color_depth d) {
    return static_cast<ColorDepth>(static_cast<int>(d));
}

inline Color to_color(const tc_color& c) {
    switch (c.kind) {
        case TC_COLOR_ANSI16: return Color::ansi16(c.index);
        case TC_COLOR_INDEXED: return Color::indexed(c.index);
        case TC_COLOR_RGB: return Color::rgb(c.r, c.g, c.b);
        default: return Color::none();
    }
}

inline Style to_style(const tc_style* s) {
    Style st;
    if (!s) return st;
    st.set_fg(to_color(s->fg));
    st.set_bg(to_color(s->bg));
    static const Attr kBits[] = {Attr::Bold, Attr::Dim, Attr::Italic, Attr::Underline,
                                 Attr::Reverse};
    for (Attr a : kBits) {
        if ((s->attrs & mask_of(a)) != 0) st.set_attr(a, true);
    }
    return st;
}

inline api::ApiOptions to_api_opts(const tc_api_opts* o) {
    api::ApiOptions opts;
    if (!o) return opts;
    if (o->width) opts.width(static_cast<std::uint16_t>(o->width));
    if (o->height) opts.height(static_cast<std::uint16_t>(o->height));
    if (o->has_depth) opts.color_depth(to_depth(o->depth));
    opts.unicode(o->unicode != 0);
    opts.newline(o->newline != 0);
    opts.reset_at_end(o->reset_at_end != 0);
    return opts;
}

// 展示选项 → 渲染选项（宽/高/深度）。尺寸越界直接判非法，不静默截断。
inline bool to_render_opts(const tc_plot_opts* o, api::ApiOptions& out) {
    if (!o) return true;
    if (o->width) {
        if (o->width > 0xFFFFu) return false;
        out.width(static_cast<std::uint16_t>(o->width));
    }
    if (o->height) {
        if (o->height > 0xFFFFu) return false;
        out.height(static_cast<std::uint16_t>(o->height));
    }
    // depth==TC_DEPTH_NONE(0) 视为"取默认"（ApiOptions 默认 TrueColor）。
    if (o->depth != TC_DEPTH_NONE) out.color_depth(to_depth(o->depth));
    return true;
}

// ── 错误 / 输出辅助 ──────────────────────────────────────────────────────────

inline void set_err(tc_error_handle* err, const std::string& msg) {
    if (err) *err = reinterpret_cast<tc_error_handle>(new (std::nothrow) std::string(msg));
}

inline tc_status fail(tc_error_handle* err, const std::string& msg, tc_status code) {
    set_err(err, msg);
    return code;
}

// 把 std::string 拷进 C 侧堆串（new[]，配 tc_free_string）。
inline tc_status store_string(const std::string& s, char** out, tc_error_handle* err,
                              tc_status base_code) {
    char* buf = new (std::nothrow) char[s.size() + 1];
    if (!buf) return fail(err, "out of memory", base_code);
    if (!s.empty()) std::memcpy(buf, s.data(), s.size());
    buf[s.size()] = '\0';
    *out = buf;
    return 0;
}

inline std::string_view sv_of(const char* p, std::uint64_t n) {
    return (p && n) ? std::string_view(p, static_cast<std::size_t>(n)) : std::string_view();
}

// ── 图表：惰性构建 + 缓存 ───────────────────────────────────────────────────

// 把模型灌进具体图种。只在缓存缺失时调用一次（之后由缓存持有，不再深拷贝）。
std::unique_ptr<chart::Chart> build_chart(ChartData& d) {
    switch (d.kind) {
        case ChartData::Kind::Line: {
            auto ch = std::make_unique<chart::LineChart>();
            ch->set_series(d.series);
            ch->set_auto_domain(d.auto_domain, d.include_zero);
            ch->set_emphasize_zero(d.emphasize_zero);
            ch->set_shade_negative(d.shade_negative);
            if (d.y_domain) ch->set_y_domain(d.y_domain->first, d.y_domain->second);
            if (!d.x_ticks.empty()) ch->set_x_ticks(d.x_ticks);
            return ch;
        }
        case ChartData::Kind::Scatter: {
            auto ch = std::make_unique<chart::ScatterChart>();
            ch->set_series(d.series);
            ch->set_auto_domain(d.auto_domain, d.include_zero);
            return ch;
        }
        case ChartData::Kind::Bar: {
            auto ch = std::make_unique<chart::BarChart>();
            ch->set_values(d.values, d.categories, d.bar_color, d.bar_name);
            if (d.y_domain) ch->set_y_domain(d.y_domain->first, d.y_domain->second);
            return ch;
        }
        case ChartData::Kind::Pie: {
            auto ch = std::make_unique<chart::PieChart>();
            chart::PieOptions po;
            po.show_legend = d.legend;  // 图例在 PieOptions 里，构建期定
            ch->set_options(po);
            ch->set_values(d.values, d.categories);  // 标签已存于 categories
            return ch;
        }
        case ChartData::Kind::Sparkline: {
            auto ch = std::make_unique<chart::SparklineChart>();
            ch->set_values(d.values);
            return ch;
        }
    }
    return nullptr;
}

chart::Chart& ensure_chart(ChartData& d) {
    if (!d.cached) d.cached = build_chart(d);
    return *d.cached;
}

// 展示属性按次应用：可重复调用，不进缓存（缓存里只有模型）。
// 图例开关在基类上不可虚调用（各图种各自实现），故按 kind 下发到具体类型；
// 不为 C 面的便利去给 C++ 基类加虚函数（接口方向不能反）。
chart::Chart& apply_display(ChartData& d, const tc_plot_opts* o) {
    d.legend = o && (o->show_legend != 0);  // 饼图在构建期据此设 PieOptions
    chart::Chart& ch = ensure_chart(d);
    ch.set_title(o && o->title ? std::string(o->title) : std::string());
    switch (d.kind) {
        case ChartData::Kind::Line:
            static_cast<chart::LineChart&>(ch).set_show_legend(d.legend);
            break;
        case ChartData::Kind::Scatter:
            static_cast<chart::ScatterChart&>(ch).set_show_legend(d.legend);
            break;
        case ChartData::Kind::Bar:
            static_cast<chart::BarChart&>(ch).set_show_legend(d.legend);
            break;
        // Pie：图例在构建期设过；Sparkline：无图例概念
        default:
            break;
    }
    return ch;
}

// 空数据显式报错：不产出"看起来像空白的图"。
bool has_data(const ChartData& d) {
    // 值类图种（柱状 / 饼图 / 迷你图）看 values；系列类（折线 / 散点）看 series。
    if (d.kind == ChartData::Kind::Bar || d.kind == ChartData::Kind::Pie ||
        d.kind == ChartData::Kind::Sparkline) {
        return !d.values.empty();
    }
    for (const auto& s : d.series) {
        if (!s.points.empty()) return true;
    }
    return false;
}

// 轴标签不进 C++ 核心 Chart 基类（核心把 Chart 定义为"能画到 Canvas 的抽象"，
// 轴标签语义属于 Dataset 层）。这里在 C-ABI 层把"图"和"轴标签"分层合成：
// 图占主区，轴标签画在预留边距里，互不覆盖。
//
// 返回 false 表示 width/height 非法（调用方转为 TC_ERR_INVALID_ARG）。
bool build_plot_with_labels(ChartData& d, const tc_plot_opts* o, CellBuffer& out) {
    const std::uint32_t req_cols = (o && o->width) ? o->width : 80u;
    const std::uint32_t req_rows = (o && o->height) ? o->height : 24u;
    if (req_cols == 0 || req_rows == 0 || req_cols > 0xFFFF || req_rows > 0xFFFF) return false;

    canvas::Canvas plot_cv(req_cols, req_rows);
    chart::DrawContext ctx{plot_cv};
    chart::Chart& ch = apply_display(d, o);
    ch.draw(ctx);
    CellBuffer plot = plot_cv.to_buffer();

    const bool has_x = !d.x_label.empty();
    const bool has_y = !d.y_label.empty();
    const std::uint32_t y_margin = has_y ? 2u : 0u;  // 左侧留 2 列给纵向 y 轴标题
    const std::uint32_t x_margin = has_x ? 2u : 0u;  // 底部留 2 行给 x 轴标题
    const std::uint32_t out_w = req_cols + y_margin;
    const std::uint32_t out_h = req_rows + x_margin;

    std::vector<render::Layer> layers;
    layers.push_back(render::Layer{&plot, static_cast<std::int32_t>(y_margin), 0});

    CellBuffer xlab, ylab;
    if (has_x) {
        canvas::Canvas lab(req_cols, x_margin);
        const std::string& s = d.x_label;
        const std::uint32_t start = (req_cols > s.size())
                                       ? (req_cols - static_cast<std::uint32_t>(s.size())) / 2u
                                       : 0u;
        lab.draw_text(Point{static_cast<double>(start), x_margin > 1 ? 1.0 : 0.0}, s);
        xlab = lab.to_buffer();
        layers.push_back(render::Layer{&xlab, static_cast<std::int32_t>(y_margin),
                                      static_cast<std::int32_t>(req_rows)});
    }
    if (has_y) {
        canvas::Canvas lab(y_margin, req_rows);
        const std::string& s = d.y_label;
        const long mid = static_cast<long>(req_rows) / 2;
        const long top = mid + static_cast<long>(s.size()) / 2;  // 第一个字的最高行
        for (std::size_t i = 0; i < s.size(); ++i) {
            const long row = top - static_cast<long>(i);
            if (row < 0) break;
            lab.draw_text(Point{0.0, static_cast<double>(row)}, std::string(1, s[i]));
        }
        ylab = lab.to_buffer();
        layers.push_back(render::Layer{&ylab, 0, 0});
    }
    out = CellBuffer(out_w, out_h);
    render::Compositor::composite(layers, out);
    return true;
}

// SVG 走矢量 Scene 路径（与 Canvas 不同）。Text 不计入 Scene::bounds()，而 SVG
// viewBox 默认用 bounds() 自动定——所以框外的轴标签会被裁掉。这里在 C-ABI 层追加
// TextOp 画标签，并用两个退化 MoveTo（零长、即便被描边也不可见）撑开包围盒。
void decorate_scene_labels(scene::Scene& scene, const ChartData& d) {
    if (d.x_label.empty() && d.y_label.empty()) return;
    const Rect b = scene.bounds();  // 仅几何，即"图"的包围盒
    if (!d.x_label.empty()) {
        const double cx = b.x + b.w / 2.0 - static_cast<double>(d.x_label.size()) / 2.0;
        scene.push(scene::Command{scene::TextOp{Point{cx, b.bottom() + 1.0}, d.x_label, Style{}}});
    }
    if (!d.y_label.empty()) {
        const double cy = b.y + b.h / 2.0 + static_cast<double>(d.y_label.size()) / 2.0;
        for (std::size_t i = 0; i < d.y_label.size(); ++i) {
            scene.push(scene::Command{
                scene::TextOp{Point{b.x - 1.0, cy - static_cast<double>(i)},
                              std::string(1, d.y_label[i]), Style{}}});
        }
    }
    // 用极值角的零长 LineTo 撑开 bounds（autofit 据此定 viewBox）。留足左右边距，
    // 让 y 轴标题（位于 b.x-1）落在框内；bounds() 只统计 MoveTo/LineTo/Arc。
    const double m = 5.0;
    const double minx = b.x - m, miny = b.y - m;
    const double maxx = b.right() + m, maxy = b.bottom() + m;
    auto corner = [&](double x, double y) {
        scene.push(scene::Command{scene::MoveTo{Point{x, y}}});
        scene.push(scene::Command{scene::LineTo{Point{x, y}}});
    };
    corner(minx, miny);
    corner(maxx, maxy);
}

// 统一渲染入口：三种图种走同一条路（不重复造渲染逻辑）。
inline tc_status render_chart_bytes(ChartData& c, const tc_plot_opts* opts, std::string& out,
                                    tc_error_handle* err) {
    if (!has_data(c)) return fail(err, "empty chart data", TC_ERR_INVALID_ARG);
    CellBuffer cells;
    if (!build_plot_with_labels(c, opts, cells)) {
        return fail(err, "width/height out of range", TC_ERR_INVALID_ARG);
    }
    const ColorDepth depth = (opts && opts->depth != TC_DEPTH_NONE) ? to_depth(opts->depth)
                                                                   : ColorDepth::TrueColor;
    render::Renderer renderer(depth);
    out = renderer.render_full(cells);
    return 0;
}

}  // namespace

extern "C" {

// ── 版本 / 初始化 ───────────────────────────────────────────────────────────

const char* tc_version_string(void) { return TERMCHART_VERSION_STRING; }

void tc_init(void) { api::init_core(); }
uint8_t tc_core_initialized(void) { return api::core_initialized() ? 1u : 0u; }

tc_api_opts tc_api_opts_default(void) {
    tc_api_opts o{};
    o.width = 0;
    o.height = 0;
    o.depth = TC_DEPTH_TRUECOLOR;
    o.has_depth = 1;
    o.unicode = 1;
    o.newline = 1;
    o.reset_at_end = 1;
    return o;
}

tc_style tc_style_default(void) {
    tc_style s{};
    s.fg.kind = TC_COLOR_NONE;
    s.bg.kind = TC_COLOR_NONE;
    s.attrs = 0;
    return s;
}

// ── 文本渲染（对齐 api::render_line / print_line / hello）───────────────────

tc_status tc_render_line(const char* text, const tc_style* style, const tc_api_opts* opts,
                         char** out_ansi, tc_error_handle* err) {
    if (!text || !out_ansi) return fail(err, "null text or output buffer", 2);
    const Style st = to_style(style);
    const api::ApiOptions o = to_api_opts(opts);
    auto r = api::render_line(std::string_view(text), st, o);
    if (!r.has_value()) {
        return fail(err, std::string("render_line failed (") + describe(r.error()) + ")", 3);
    }
    return store_string(r.value(), out_ansi, err, 4);
}

tc_status tc_print_line(const char* text, const tc_style* style, const tc_api_opts* opts,
                        tc_error_handle* err) {
    if (!text) return fail(err, "null text", 2);
    const Status s = api::print_line(std::string_view(text), to_style(style), to_api_opts(opts));
    if (!is_ok(s)) return fail(err, std::string("print_line failed (") + describe(s) + ")", 5);
    return 0;
}

tc_status tc_hello(const tc_api_opts* opts, tc_error_handle* err) {
    const Status s = api::hello(to_api_opts(opts));
    if (!is_ok(s)) return fail(err, std::string("hello failed (") + describe(s) + ")", 5);
    return 0;
}

// ── 图表句柄 ────────────────────────────────────────────────────────────────

tc_chart_handle tc_chart_create_line(void) {
    auto* c = new (std::nothrow) ChartData();
    if (c) c->kind = ChartData::Kind::Line;
    return reinterpret_cast<tc_chart_handle>(c);
}
tc_chart_handle tc_chart_create_scatter(void) {
    auto* c = new (std::nothrow) ChartData();
    if (c) c->kind = ChartData::Kind::Scatter;
    return reinterpret_cast<tc_chart_handle>(c);
}
tc_chart_handle tc_chart_create_bar(void) {
    auto* c = new (std::nothrow) ChartData();
    if (c) c->kind = ChartData::Kind::Bar;
    return reinterpret_cast<tc_chart_handle>(c);
}
tc_chart_handle tc_chart_create_pie(void) {
    auto* c = new (std::nothrow) ChartData();
    if (c) c->kind = ChartData::Kind::Pie;
    return reinterpret_cast<tc_chart_handle>(c);
}
tc_chart_handle tc_chart_create_sparkline(void) {
    auto* c = new (std::nothrow) ChartData();
    if (c) c->kind = ChartData::Kind::Sparkline;
    return reinterpret_cast<tc_chart_handle>(c);
}
void tc_chart_free(tc_chart_handle chart) { delete reinterpret_cast<ChartData*>(chart); }

tc_status tc_chart_add_series(tc_chart_handle chart, const tc_series* series, uint64_t count,
                              tc_error_handle* err) {
    if (!chart) return fail(err, "null chart", TC_ERR_INVALID_HANDLE);
    if (!series && count) return fail(err, "null series array", TC_ERR_INVALID_ARG);
    auto* c = reinterpret_cast<ChartData*>(chart);
    if (c->kind == ChartData::Kind::Bar) {
        return fail(err, "bar chart takes values/labels: use tc_chart_add_bar",
                    TC_ERR_UNSUPPORTED);
    }
    const std::size_t base = c->series.size();
    c->series.reserve(base + static_cast<std::size_t>(count));
    for (uint64_t i = 0; i < count; ++i) {
        const tc_series& in = series[i];
        if (in.n == 0 || !in.x || !in.y) {
            // 批量入口的原子性：任何一条非法就整批不落地，已追加的回滚。
            c->series.resize(base);
            c->invalidate();
            return fail(err, in.n == 0 ? "empty series (n=0)" : "null x/y with n>0",
                        TC_ERR_INVALID_ARG);
        }
        chart::Series out;
        out.name = (in.name && in.name[0]) ? std::string(in.name)
                                           : ("series " + std::to_string(base + i + 1));
        if (in.color.kind != TC_COLOR_NONE) out.color = to_color(in.color);
        out.points.reserve(static_cast<std::size_t>(in.n));
        for (uint64_t k = 0; k < in.n; ++k) out.points.emplace_back(in.x[k], in.y[k]);
        c->series.push_back(std::move(out));
    }
    c->invalidate();
    return 0;
}

tc_status tc_chart_add_points(tc_chart_handle chart, const double* x, const double* y,
                              uint64_t n) {
    tc_series one{};
    one.x = x;
    one.y = y;
    one.n = n;
    return tc_chart_add_series(chart, &one, 1, nullptr);
}

tc_status tc_chart_add_bar(tc_chart_handle chart, const double* values, uint64_t n,
                           const char* const* labels, uint64_t n_labels) {
    return tc_chart_set_values(chart, values, n, labels, n_labels, nullptr);
}

// 通用"值 + 类别标签"入口：柱状 / 饼图 / 迷你图共用一套数据形状。
// 对只收系列的图种（折线 / 散点）显式报 Unsupported，不静默丢弃数据。
tc_status tc_chart_set_values(tc_chart_handle chart, const double* values, uint64_t n,
                              const char* const* labels, uint64_t n_labels,
                              tc_error_handle* err) {
    if (!chart) return fail(err, "null chart", TC_ERR_INVALID_HANDLE);
    if (!values && n) return fail(err, "null values with n>0", TC_ERR_INVALID_ARG);
    auto* c = reinterpret_cast<ChartData*>(chart);
    if (c->kind != ChartData::Kind::Bar && c->kind != ChartData::Kind::Pie &&
        c->kind != ChartData::Kind::Sparkline) {
        return fail(err, "this chart kind takes series, not values", TC_ERR_UNSUPPORTED);
    }
    c->values.clear();
    if (n) c->values.assign(values, values + n);
    c->categories.clear();
    if (labels && n_labels > 0) {
        c->categories.reserve(static_cast<std::size_t>(n_labels));
        for (uint64_t i = 0; i < n_labels; ++i) {
            c->categories.emplace_back(labels[i] ? labels[i] : "");
        }
    }
    c->invalidate();
    return 0;
}

tc_status tc_chart_set_prop(tc_chart_handle chart, tc_chart_prop prop, const tc_value* value,
                            tc_error_handle* err) {
    if (!chart) return fail(err, "null chart", TC_ERR_INVALID_HANDLE);
    auto* c = reinterpret_cast<ChartData*>(chart);
    const tc_value v = value ? *value : tc_value{};
    const bool clear = is_clear(v);
    bool b = false;

    switch (prop) {
        case TC_PROP_Y_DOMAIN: {
            if (c->kind == ChartData::Kind::Scatter) {
                return fail(err, "scatter chart has no y-domain", TC_ERR_UNSUPPORTED);
            }
            if (clear) {
                c->y_domain.reset();
                break;
            }
            if (v.kind != TC_VAL_DOUBLE_ARRAY || v.n != 2) {
                return fail(err, "Y_DOMAIN expects 2 doubles {lo, hi}", TC_ERR_INVALID_ARG);
            }
            if (!std::isfinite(v.p[0]) || !std::isfinite(v.p[1]) || !(v.p[1] > v.p[0])) {
                return fail(err, "Y_DOMAIN requires finite hi > lo", TC_ERR_INVALID_ARG);
            }
            c->y_domain = std::make_pair(v.p[0], v.p[1]);
            break;
        }
        case TC_PROP_AUTO_DOMAIN:
        case TC_PROP_INCLUDE_ZERO: {
            if (c->kind == ChartData::Kind::Bar) {
                return fail(err, "bar chart is always auto-domain", TC_ERR_UNSUPPORTED);
            }
            if (clear) return fail(err, "bool property has no clear state", TC_ERR_INVALID_ARG);
            if (!bool_of(v, b)) return fail(err, "expects bool", TC_ERR_INVALID_ARG);
            if (prop == TC_PROP_AUTO_DOMAIN) {
                c->auto_domain = b;
            } else {
                c->include_zero = b;
            }
            break;
        }
        case TC_PROP_EMPHASIZE_ZERO:
        case TC_PROP_SHADE_NEGATIVE: {
            if (c->kind != ChartData::Kind::Line) {
                return fail(err, "line chart only", TC_ERR_UNSUPPORTED);
            }
            if (clear) return fail(err, "bool property has no clear state", TC_ERR_INVALID_ARG);
            if (!bool_of(v, b)) return fail(err, "expects bool", TC_ERR_INVALID_ARG);
            if (prop == TC_PROP_EMPHASIZE_ZERO) {
                c->emphasize_zero = b;
            } else {
                c->shade_negative = b;
            }
            break;
        }
        case TC_PROP_X_TICKS: {
            if (c->kind != ChartData::Kind::Line) {
                return fail(err, "line chart only", TC_ERR_UNSUPPORTED);
            }
            if (clear) {
                c->x_ticks.clear();
                break;
            }
            if (v.kind != TC_VAL_DOUBLE_ARRAY) {
                return fail(err, "X_TICKS expects double array", TC_ERR_INVALID_ARG);
            }
            c->x_ticks.assign(v.p, v.p + v.n);
            break;
        }
        case TC_PROP_X_LABEL:
        case TC_PROP_Y_LABEL: {
            if (clear) {
                if (prop == TC_PROP_X_LABEL)
                    c->x_label.clear();
                else
                    c->y_label.clear();
                break;
            }
            if (v.kind != TC_VAL_STRING || !v.s) {
                return fail(err, "X/Y_LABEL expects a string", TC_ERR_INVALID_ARG);
            }
            if (prop == TC_PROP_X_LABEL)
                c->x_label = v.s;
            else
                c->y_label = v.s;
            break;
        }
        default:
            return fail(err, "unknown chart property", TC_ERR_INVALID_ARG);
    }
    c->invalidate();
    return 0;
}

tc_status tc_chart_render_ansi(tc_chart_handle chart, const tc_plot_opts* opts, char** out_ansi,
                               tc_error_handle* err) {
    if (!chart || !out_ansi) return fail(err, "null chart or output buffer", TC_ERR_INVALID_ARG);
    std::string bytes;
    const tc_status st = render_chart_bytes(*reinterpret_cast<ChartData*>(chart), opts, bytes, err);
    if (st != 0) return st;
    return store_string(bytes, out_ansi, err, TC_ERR_NOMEM);
}

tc_status tc_chart_print(tc_chart_handle chart, const tc_plot_opts* opts, tc_error_handle* err) {
    if (!chart) return fail(err, "null chart", TC_ERR_INVALID_HANDLE);
    std::string bytes;
    const tc_status st = render_chart_bytes(*reinterpret_cast<ChartData*>(chart), opts, bytes, err);
    if (st != 0) return st;
    const Status s = api::write_stdout(bytes);
    if (!is_ok(s)) return fail(err, std::string("write_stdout failed (") + describe(s) + ")",
                               TC_ERR_IO);
    return 0;
}

tc_status tc_chart_to_svg(tc_chart_handle chart, const tc_plot_opts* opts, char* buf,
                          uint64_t cap, uint64_t* out_len, tc_error_handle* err) {
    if (!chart || !out_len) return fail(err, "null chart or out_len", TC_ERR_INVALID_ARG);
    auto* c = reinterpret_cast<ChartData*>(chart);
    if (!has_data(*c)) return fail(err, "empty chart data", TC_ERR_INVALID_ARG);
    std::uint32_t w = 80;
    std::uint32_t h = 24;
    if (opts && opts->width) w = opts->width;
    if (opts && opts->height) h = opts->height;
    chart::Chart& ch = apply_display(*c, opts);
    const auto sc = api::chart_to_scene(ch, w, h);
    if (!sc.has_value()) {
        return fail(err, std::string("chart_to_scene failed (") + describe(sc.error()) + ")",
                    TC_ERR_RENDER);
    }
    scene::Scene scene = std::move(sc.value());
    decorate_scene_labels(scene, *c);
    backend::vector::VectorOptions vo;
    vo.font_size = 1.0;  // 以"格"为场景单位：字号设 1.0，避免被 scale 放大 scale 倍
    backend::vector::SvgBackend svg;
    svg.set_options(vo);
    const std::string s = backend::vector::render_vector(svg, scene);
    if (s.empty()) return fail(err, "svg encode failed", TC_ERR_RENDER);
    *out_len = static_cast<uint64_t>(s.size());
    if (!buf || cap == 0) return 0;  // 第一段：只问所需长度
    if (cap < s.size()) return TC_ERR_BUFFER_TOO_SMALL;
    std::memcpy(buf, s.data(), s.size());
    return 0;
}

// ── 输出 ────────────────────────────────────────────────────────────────────

tc_status tc_write_stdout(const char* bytes, uint64_t n, tc_error_handle* err) {
    if (!bytes && n) return fail(err, "null bytes", 2);
    const Status s = api::write_stdout(sv_of(bytes, n));
    if (!is_ok(s)) return fail(err, std::string("write_stdout failed (") + describe(s) + ")", 5);
    return 0;
}

tc_status tc_write_stderr(const char* bytes, uint64_t n, tc_error_handle* err) {
    if (!bytes && n) return fail(err, "null bytes", 2);
    const Status s = api::write_stderr(sv_of(bytes, n));
    if (!is_ok(s)) return fail(err, std::string("write_stderr failed (") + describe(s) + ")", 5);
    return 0;
}

tc_status tc_flush_stdout(tc_error_handle* err) {
    const Status s = api::flush_stdout();
    if (!is_ok(s)) return fail(err, std::string("flush_stdout failed (") + describe(s) + ")", 5);
    return 0;
}

// ── AutoViz ─────────────────────────────────────────────────────────────────

tc_status tc_auto_plot(const char* filepath, tc_error_handle* err) {
    if (!filepath) return fail(err, "null filepath", 2);
    const Status s = api::auto_plot(std::string_view(filepath));
    if (!is_ok(s)) return fail(err, std::string("auto_plot failed (") + describe(s) + ")", 3);
    return 0;
}

// ── 释放 / 错误消息 ─────────────────────────────────────────────────────────

void tc_free_string(char* s) { delete[] s; }

tc_status tc_chart_to_png(tc_chart_handle chart, const tc_plot_opts* o, char* buf,
                          uint64_t cap, uint64_t* out_len, tc_error_handle* err) {
    if (!chart) return fail(err, "null chart", TC_ERR_INVALID_HANDLE);
    auto* c = reinterpret_cast<ChartData*>(chart);
    if (!has_data(*c)) return fail(err, "empty chart data", TC_ERR_INVALID_ARG);
    const std::uint32_t cols = (o && o->width) ? o->width : 80u;
    const std::uint32_t rows = (o && o->height) ? o->height : 24u;
    if (cols == 0 || rows == 0 || cols > 0xFFFF || rows > 0xFFFF)
        return fail(err, "invalid width/height", TC_ERR_INVALID_ARG);

    chart::Chart& ch = apply_display(*c, o);
    CellBuffer cells;
    if (!build_plot_with_labels(*c, o, cells)) {
        return fail(err, "invalid width/height", TC_ERR_INVALID_ARG);
    }
    gui::CellBitmapOptions copts{};  // 默认 9×16、深色底
    image::Bitmap bmp = gui::render_cells_to_bitmap(cells, copts);
    std::string png = image::encode(bmp, image::ImageFormat::Png);
    if (png.empty()) return fail(err, "png encode failed", TC_ERR_RENDER);

    if (cap == 0) {
        if (out_len) *out_len = png.size();
        return TC_OK;
    }
    if (png.size() > cap) {
        if (out_len) *out_len = png.size();  // 回写所需长度，便于调用方扩容
        return TC_ERR_BUFFER_TOO_SMALL;
    }
    std::memcpy(buf, png.data(), png.size());
    if (out_len) *out_len = png.size();
    return TC_OK;
}

void tc_error_message(tc_error_handle err, char* buf, uint64_t cap) {
    if (!err || !buf || cap == 0) return;
    const auto* msg = reinterpret_cast<std::string*>(err);
    const size_t n = static_cast<size_t>(cap) - 1;
    std::strncpy(buf, msg->c_str(), n);
    buf[n] = '\0';
}
void tc_error_free(tc_error_handle err) { delete reinterpret_cast<std::string*>(err); }

}  // extern "C"
