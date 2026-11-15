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
#include "termchart/chart/cartesian/area.hpp"
#include "termchart/chart/cartesian/bar_chart.hpp"
#include "termchart/chart/cartesian/contour.hpp"
#include "termchart/chart/cartesian/heatmap.hpp"
#include "termchart/chart/cartesian/hist.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/chart/cartesian/mosaic.hpp"
#include "termchart/chart/cartesian/parallel.hpp"
#include "termchart/chart/cartesian/qq.hpp"
#include "termchart/chart/cartesian/scatter_chart.hpp"
#include "termchart/chart/cartesian/sparkline.hpp"
#include "termchart/chart/cartesian/step.hpp"
#include "termchart/chart/cartesian/streamgraph.hpp"
#include "termchart/chart/cartesian/violin.hpp"
#include "termchart/chart/context.hpp"
#include "termchart/chart/flow/stream.hpp"
#include "termchart/chart/polar/donut.hpp"
#include "termchart/chart/polar/pie.hpp"
#include "termchart/chart/polar/polar_bar.hpp"
#include "termchart/chart/polar/radar.hpp"
#include "termchart/chart/polar/rose.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/chart/statistical/autocorr.hpp"
#include "termchart/chart/statistical/corr.hpp"
#include "termchart/chart/statistical/distribution.hpp"
#include "termchart/chart/statistical/ecdf.hpp"
#include "termchart/chart/statistical/pca.hpp"
#include "termchart/chart/statistical/regression.hpp"
#include "termchart/chart/statistical/seasonal.hpp"
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

struct ChartData;  // 前向声明：ChartSpec::apply 与 ChartData::spec 互相引用

// 数据形状：决定哪个数据入口能灌、has_data 看哪个容器。加图种时只需在 spec 表里
// 声明形状，不必再改 add_series / set_values / set_matrix / has_data 的 if-else。
enum class DataShape {
    Series,  // 多系列（line / scatter / streamgraph / stream）
    Values,  // 一组 double + 类别标签（bar / pie / hist / polar-* …）
    Points,  // 一组 (x,y)（area / step / heatmap / regression …）
    Matrix,  // 二维表（contour / corr / mosaic / violin / pca / parallel）
};

// 能力位：模型属性按图种支持情况显式拒绝，不静默忽略。
struct Capabilities {
    bool y_domain = false;
    bool auto_domain = false;   // 同时控制 include_zero
    bool emphasize_zero = false;
    bool shade_negative = false;
    bool x_ticks = false;
};

// 图种描述：工厂 + 数据灌入器 + 形状 + 能力 + 图例下发方式。
// 这是"加图种只加一行"的唯一落点——核心零改动、C 头零改动、渲染层零改动。
struct ChartSpec {
    const char* name;
    std::unique_ptr<chart::Chart> (*make)();
    void (*apply)(chart::Chart&, const ChartData&);
    DataShape shape;
    Capabilities caps;
    void (*set_legend)(chart::Chart&, bool);  // 可为 nullptr（该图种无图例概念）
};

// 句柄背后只有这一种数据：模型（按 shape 装的通用容器）+ 惰性构建的图表对象。
//
// 惰性 + 缓存是**性能**的关键：同一张图先 render_ansi 再 print 只构建一次；
// 任何模型改动都经 invalidate() 丢弃缓存，不存在"改了没生效"。
struct ChartData {
    std::string kind = "line";  // 图种名，对应 ChartSpec::name

    // 通用数据容器：按 spec.shape 选用其一（其余为空）
    std::vector<chart::Series> series;        // Series / Points 形状（Points 只用第一条的 points）
    std::vector<double> values;               // Values 形状
    std::vector<std::string> categories;      // Values 形状的类别 / 标签
    std::vector<std::vector<double>> matrix;  // Matrix 形状
    std::vector<std::string> row_labels;      // Matrix 形状的可选行/列标签
    std::vector<std::string> col_labels;
    std::string bar_name = "series";
    Color bar_color = chart::default_color(chart::DefaultColor::Accent);

    // 模型属性（展示属性在 tc_plot_opts 里按次传入，这里不重复存）
    std::optional<std::pair<double, double>> y_domain;
    bool auto_domain = true;
    bool include_zero = false;
    bool emphasize_zero = false;
    bool shade_negative = false;
    bool legend = false;  // 展示属性，按次渲染时设置
    std::vector<double> x_ticks;

    // 轴标签：纯 C-ABI 层语义，不在 C++ 核心 Chart 上。渲染时后置绘制到边距里。
    std::string x_label;
    std::string y_label;

    std::unique_ptr<chart::Chart> cached;
    void invalidate() noexcept { cached.reset(); }

    const ChartSpec& spec() const noexcept;  // 定义见 spec 表之后
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

// ── 图种表：工厂 / 数据灌入 / 能力，一处登记 ─────────────────────────────────
// 模板让同形状图种共享 applier；新图种通常只需在 kSpecs 里加一行。

template <class T>
std::unique_ptr<chart::Chart> make_chart() {
    return std::make_unique<T>();
}

// Series 形状：line / scatter / streamgraph / stream 都用 set_series。
template <class T>
void apply_series(chart::Chart& ch, const ChartData& d) {
    static_cast<T&>(ch).set_series(d.series);
}

// Values 形状：不同图种 setter 末参不同，用几个模板分流。
template <class T>
void apply_values_labels(chart::Chart& ch, const ChartData& d) {  // (values, labels)
    static_cast<T&>(ch).set_values(d.values, d.categories);
}
template <class T>
void apply_values_color(chart::Chart& ch, const ChartData& d) {  // (values, color)
    static_cast<T&>(ch).set_values(d.values, d.bar_color);
}
template <class T>
void apply_values_color_name(chart::Chart& ch, const ChartData& d) {  // (values, color, name)
    static_cast<T&>(ch).set_values(d.values, d.bar_color, d.bar_name);
}
template <class T>
void apply_values_plain(chart::Chart& ch, const ChartData& d) {  // (values) 单系列
    static_cast<T&>(ch).set_values(d.values);
}
inline void apply_qq(chart::Chart& ch, const ChartData& d) {  // QQChart::set_sample
    static_cast<chart::QQChart&>(ch).set_sample(d.values);
}

// Points 形状：series[0].points 作为 (x,y) 序列灌入。
template <class T>
void apply_points(chart::Chart& ch, const ChartData& d) {
    std::vector<Point> pts;
    if (!d.series.empty()) pts = d.series.front().points;
    static_cast<T&>(ch).set_values(std::move(pts), d.bar_color, d.bar_name);
}
template <class T>
void apply_points_colormap(chart::Chart& ch, const ChartData& d) {  // heatmap: 第二参是 colormap
    std::vector<Point> pts;
    if (!d.series.empty()) pts = d.series.front().points;
    static_cast<T&>(ch).set_values(std::move(pts));  // 用默认 colormap
}
// RegressionChart::set_points（方法名不同）
inline void apply_regression_points(chart::Chart& ch, const ChartData& d) {
    std::vector<Point> pts;
    if (!d.series.empty()) pts = d.series.front().points;
    static_cast<chart::RegressionChart&>(ch).set_points(std::move(pts), d.bar_color, d.bar_name);
}

// Matrix 形状：各图种矩阵 setter 形态不同，逐个适配（每处 3-5 行）。
inline void apply_contour(chart::Chart& ch, const ChartData& d) {
    static_cast<chart::ContourChart&>(ch).set_field(d.matrix);
}
inline void apply_corr(chart::Chart& ch, const ChartData& d) {
    static_cast<chart::CorrMatrixChart&>(ch).set_matrix(d.row_labels, d.matrix);
}
inline void apply_mosaic(chart::Chart& ch, const ChartData& d) {
    static_cast<chart::MosaicChart&>(ch).set_counts(d.matrix, d.row_labels, d.col_labels);
}
inline void apply_violin(chart::Chart& ch, const ChartData& d) {
    static_cast<chart::ViolinChart&>(ch).set_data(d.matrix);
}
inline void apply_pca(chart::Chart& ch, const ChartData& d) {
    static_cast<chart::PcaChart&>(ch).set_data(d.matrix);
}
inline void apply_parallel(chart::Chart& ch, const ChartData& d) {
    static_cast<chart::ParallelChart&>(ch).set_axes(d.row_labels, d.matrix);
}

// 图例下发：能 set_show_legend 的图种走这个模板；Pie 的选项在构建期另设。
template <class T>
void set_legend_of(chart::Chart& ch, bool on) {
    static_cast<T&>(ch).set_show_legend(on);
}

// 能力快捷构造
inline Capabilities caps_xy() {
    return Capabilities{true, true, true, true, true};
}

// spec 表。列：name / make / apply / shape / caps / legend-setter。
// 加一个图种 = 加一行；ABI、渲染层、影子模型其余部分都不动。
const std::vector<ChartSpec> kSpecs = {
    // ── Series（tc_chart_add_series / add_points）──
    {"line",        make_chart<chart::LineChart>,        apply_series<chart::LineChart>,        DataShape::Series, caps_xy(),   set_legend_of<chart::LineChart>},
    {"scatter",     make_chart<chart::ScatterChart>,     apply_series<chart::ScatterChart>,     DataShape::Series, Capabilities{false,true,false,false,false}, set_legend_of<chart::ScatterChart>},
    {"streamgraph", make_chart<chart::StreamgraphChart>, apply_series<chart::StreamgraphChart>, DataShape::Series, Capabilities{}, nullptr},
    {"stream",      make_chart<chart::StreamChart>,   apply_series<chart::StreamChart>,     DataShape::Series, Capabilities{}, nullptr},

    // ── Values（tc_chart_set_values）──
    {"bar",        make_chart<chart::BarChart>,       nullptr, DataShape::Values, Capabilities{true,false,false,false,false}, set_legend_of<chart::BarChart>},
    {"pie",        make_chart<chart::PieChart>,       nullptr, DataShape::Values, Capabilities{}, nullptr},
    {"donut",      make_chart<chart::DonutChart>,     apply_values_labels<chart::DonutChart>,     DataShape::Values, Capabilities{}, nullptr},
    {"rose",       make_chart<chart::RoseChart>,      apply_values_labels<chart::RoseChart>,      DataShape::Values, Capabilities{}, nullptr},
    {"radar",      make_chart<chart::RadarChart>,     apply_values_labels<chart::RadarChart>,     DataShape::Values, Capabilities{}, nullptr},
    {"polar_bar",  make_chart<chart::PolarBarChart>,  apply_values_labels<chart::PolarBarChart>,  DataShape::Values, Capabilities{}, nullptr},
    {"hist",       make_chart<chart::HistChart>,      apply_values_color_name<chart::HistChart>,  DataShape::Values, Capabilities{}, set_legend_of<chart::HistChart>},
    {"sparkline",  make_chart<chart::SparklineChart>, apply_values_plain<chart::SparklineChart>,  DataShape::Values, Capabilities{}, nullptr},
    {"autocorr",   make_chart<chart::AutocorrChart>,  apply_values_color<chart::AutocorrChart>,   DataShape::Values, Capabilities{}, nullptr},
    {"distribution", make_chart<chart::DistributionChart>, apply_values_color_name<chart::DistributionChart>, DataShape::Values, Capabilities{}, nullptr},
    {"ecdf",       make_chart<chart::EcdfChart>,      apply_values_color_name<chart::EcdfChart>,  DataShape::Values, Capabilities{}, nullptr},
    {"seasonal",   make_chart<chart::SeasonalChart>,  apply_values_color<chart::SeasonalChart>,   DataShape::Values, Capabilities{}, nullptr},
    {"qq",         make_chart<chart::QQChart>,        apply_qq,                                    DataShape::Values, Capabilities{}, nullptr},

    // ── Points（tc_chart_add_points）──
    {"area",       make_chart<chart::AreaChart>,      apply_points<chart::AreaChart>,       DataShape::Points, Capabilities{}, set_legend_of<chart::AreaChart>},
    {"step",       make_chart<chart::StepChart>,      apply_points<chart::StepChart>,       DataShape::Points, Capabilities{}, set_legend_of<chart::StepChart>},
    {"heatmap",    make_chart<chart::HeatmapChart>,   apply_points_colormap<chart::HeatmapChart>, DataShape::Points, Capabilities{}, set_legend_of<chart::HeatmapChart>},
    {"regression", make_chart<chart::RegressionChart>,apply_regression_points,              DataShape::Points, Capabilities{}, set_legend_of<chart::RegressionChart>},

    // ── Matrix（tc_chart_set_matrix）──
    {"contour",    make_chart<chart::ContourChart>,   apply_contour,  DataShape::Matrix, Capabilities{}, nullptr},
    {"corr",       make_chart<chart::CorrMatrixChart>,apply_corr,     DataShape::Matrix, Capabilities{}, nullptr},
    {"mosaic",     make_chart<chart::MosaicChart>,    apply_mosaic,   DataShape::Matrix, Capabilities{}, nullptr},
    {"violin",     make_chart<chart::ViolinChart>,    apply_violin,   DataShape::Matrix, Capabilities{}, nullptr},
    {"pca",        make_chart<chart::PcaChart>,       apply_pca,      DataShape::Matrix, Capabilities{}, nullptr},
    {"parallel",   make_chart<chart::ParallelChart>,  apply_parallel, DataShape::Matrix, Capabilities{}, nullptr},
};

const ChartSpec* find_spec(std::string_view name) noexcept {
    for (const auto& s : kSpecs) {
        if (name == s.name) return &s;
    }
    return nullptr;
}

const ChartSpec& ChartData::spec() const noexcept {
    const ChartSpec* s = find_spec(kind);
    return s ? *s : kSpecs.front();  // kind 由 create 校验，理论上不会落到这里
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
// 全部经 spec 表：工厂生成对象 → applier 灌数据 → 能力位下发模型属性。
std::unique_ptr<chart::Chart> build_chart(ChartData& d) {
    const ChartSpec& sp = d.spec();
    auto ch = sp.make();
    if (!ch) return nullptr;

    // bar / pie 的数据灌入带额外构建期副作用（bar 的 y_domain、pie 的图例选项），
    // 其余图种（含全部 Series/Points/Matrix）都由 spec.apply 统一处理。
    if (d.kind == "bar") {
        static_cast<chart::BarChart&>(*ch).set_values(d.values, d.categories, d.bar_color,
                                                      d.bar_name);
        if (d.y_domain)
            static_cast<chart::BarChart&>(*ch).set_y_domain(d.y_domain->first,
                                                            d.y_domain->second);
    } else if (d.kind == "pie") {
        chart::PieOptions po;
        po.show_legend = d.legend;  // 图例在 PieOptions 里，构建期定
        static_cast<chart::PieChart&>(*ch).set_options(po);
        static_cast<chart::PieChart&>(*ch).set_values(d.values, d.categories);
    } else if (sp.apply) {
        sp.apply(*ch, d);
    }

    // 能力位下发：line/scatter 的域与零线等，全部按 spec.caps 决定，不硬编码 kind。
    if (sp.caps.auto_domain) {
        if (auto* lc = dynamic_cast<chart::LineChart*>(ch.get())) {
            lc->set_auto_domain(d.auto_domain, d.include_zero);
        } else if (auto* sc = dynamic_cast<chart::ScatterChart*>(ch.get())) {
            sc->set_auto_domain(d.auto_domain, d.include_zero);
        }
    }
    if (sp.caps.y_domain) {
        if (auto* lc = dynamic_cast<chart::LineChart*>(ch.get())) {
            if (d.y_domain) lc->set_y_domain(d.y_domain->first, d.y_domain->second);
        }
    }
    if (sp.caps.emphasize_zero) {
        auto* lc = static_cast<chart::LineChart*>(ch.get());
        lc->set_emphasize_zero(d.emphasize_zero);
        lc->set_shade_negative(d.shade_negative);
    }
    if (sp.caps.x_ticks && !d.x_ticks.empty()) {
        static_cast<chart::LineChart&>(*ch).set_x_ticks(d.x_ticks);
    }
    return ch;
}

chart::Chart& ensure_chart(ChartData& d) {
    if (!d.cached) d.cached = build_chart(d);
    return *d.cached;
}

// 展示属性按次应用：可重复调用，不进缓存（缓存里只有模型）。
// 图例开关在基类上不可虚调用（各图种各自实现），故按 spec.set_legend 下发；
// 不为 C 面的便利去给 C++ 基类加虚函数（接口方向不能反）。
chart::Chart& apply_display(ChartData& d, const tc_plot_opts* o) {
    d.legend = o && (o->show_legend != 0);
    chart::Chart& ch = ensure_chart(d);
    ch.set_title(o && o->title ? std::string(o->title) : std::string());
    if (const auto fn = d.spec().set_legend) fn(ch, d.legend);
    return ch;
}

// 空数据显式报错：不产出"看起来像空白的图"。按 spec.shape 看对应容器。
bool has_data(const ChartData& d) {
    switch (d.spec().shape) {
        case DataShape::Values: return !d.values.empty();
        case DataShape::Matrix: return !d.matrix.empty();
        case DataShape::Series:
        case DataShape::Points:
            for (const auto& s : d.series) {
                if (!s.points.empty()) return true;
            }
            return false;
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

// 通用创建：kind 字符串查 spec 表。未知名返回 NULL（调用方可查 tc_chart_kinds）。
tc_chart_handle tc_chart_create(const char* kind, tc_error_handle* err) {
    if (!kind || !find_spec(kind)) {
        fail(err, "unknown chart kind", TC_ERR_INVALID_ARG);
        return nullptr;
    }
    auto* c = new (std::nothrow) ChartData();
    if (!c) {
        fail(err, "out of memory", TC_ERR_NOMEM);
        return nullptr;
    }
    c->kind = kind;
    return reinterpret_cast<tc_chart_handle>(c);
}

// 两段式枚举可用图种（字典序，与 spec 表顺序一致）。
tc_status tc_chart_kinds(char* buf, uint64_t cap, uint64_t* out_len, tc_error_handle* err) {
    if (!out_len) return fail(err, "null out_len", TC_ERR_INVALID_ARG);
    std::string joined;
    for (const auto& s : kSpecs) {
        if (!joined.empty()) joined.push_back(' ');
        joined += s.name;
    }
    *out_len = static_cast<uint64_t>(joined.size());
    if (!buf || cap == 0) return 0;
    if (cap < joined.size()) return TC_ERR_BUFFER_TOO_SMALL;
    std::memcpy(buf, joined.data(), joined.size());
    return 0;
}

// 旧工厂保留为兼容胶水：等价于 tc_chart_create("...")，不动 ABI。
tc_chart_handle tc_chart_create_line(void)     { return tc_chart_create("line", nullptr); }
tc_chart_handle tc_chart_create_scatter(void)  { return tc_chart_create("scatter", nullptr); }
tc_chart_handle tc_chart_create_bar(void)      { return tc_chart_create("bar", nullptr); }
tc_chart_handle tc_chart_create_pie(void)      { return tc_chart_create("pie", nullptr); }
tc_chart_handle tc_chart_create_sparkline(void){ return tc_chart_create("sparkline", nullptr); }
void tc_chart_free(tc_chart_handle chart) { delete reinterpret_cast<ChartData*>(chart); }

tc_status tc_chart_add_series(tc_chart_handle chart, const tc_series* series, uint64_t count,
                              tc_error_handle* err) {
    if (!chart) return fail(err, "null chart", TC_ERR_INVALID_HANDLE);
    if (!series && count) return fail(err, "null series array", TC_ERR_INVALID_ARG);
    auto* c = reinterpret_cast<ChartData*>(chart);
    const DataShape shape = c->spec().shape;
    if (shape != DataShape::Series && shape != DataShape::Points) {
        return fail(err, "this chart kind takes values/matrix, not series", TC_ERR_UNSUPPORTED);
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

// 通用"值 + 类别标签"入口：全部 Values 形状图种共用（bar / pie / hist / polar-* …）。
// 对只收系列/矩阵的图种显式报 Unsupported，不静默丢弃数据。
tc_status tc_chart_set_values(tc_chart_handle chart, const double* values, uint64_t n,
                              const char* const* labels, uint64_t n_labels,
                              tc_error_handle* err) {
    if (!chart) return fail(err, "null chart", TC_ERR_INVALID_HANDLE);
    if (!values && n) return fail(err, "null values with n>0", TC_ERR_INVALID_ARG);
    auto* c = reinterpret_cast<ChartData*>(chart);
    if (c->spec().shape != DataShape::Values) {
        return fail(err, "this chart kind takes series/matrix, not values", TC_ERR_UNSUPPORTED);
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

// 二维矩阵入口：Matrix 形状图种（contour / corr / mosaic / violin / pca / parallel）。
// rows 为 row_count 个行指针，每行 col_count 个 double；row_labels/col_labels 可 NULL。
tc_status tc_chart_set_matrix(tc_chart_handle chart, const double* const* rows,
                              uint64_t row_count, uint64_t col_count,
                              const char* const* row_labels, uint64_t n_row_labels,
                              const char* const* col_labels, uint64_t n_col_labels,
                              tc_error_handle* err) {
    if (!chart) return fail(err, "null chart", TC_ERR_INVALID_HANDLE);
    if (!rows && row_count) return fail(err, "null rows with row_count>0", TC_ERR_INVALID_ARG);
    auto* c = reinterpret_cast<ChartData*>(chart);
    if (c->spec().shape != DataShape::Matrix) {
        return fail(err, "this chart kind takes series/values, not matrix", TC_ERR_UNSUPPORTED);
    }
    c->matrix.clear();
    c->matrix.reserve(static_cast<std::size_t>(row_count));
    for (uint64_t i = 0; i < row_count; ++i) {
        if (!rows[i] && col_count) {
            c->matrix.clear();
            c->invalidate();
            return fail(err, "null row with col_count>0", TC_ERR_INVALID_ARG);
        }
        c->matrix.emplace_back(rows[i], rows[i] + col_count);
    }
    auto fill_labels = [](std::vector<std::string>& dst, const char* const* src, uint64_t n) {
        dst.clear();
        if (!src || !n) return;
        dst.reserve(static_cast<std::size_t>(n));
        for (uint64_t i = 0; i < n; ++i) dst.emplace_back(src[i] ? src[i] : "");
    };
    fill_labels(c->row_labels, row_labels, n_row_labels);
    fill_labels(c->col_labels, col_labels, n_col_labels);
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
            if (!c->spec().caps.y_domain) {
                return fail(err, "this chart kind has no y-domain", TC_ERR_UNSUPPORTED);
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
            if (!c->spec().caps.auto_domain) {
                return fail(err, "this chart kind is always auto-domain", TC_ERR_UNSUPPORTED);
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
            if (!c->spec().caps.emphasize_zero) {
                return fail(err, "this chart kind has no zero-line emphasis", TC_ERR_UNSUPPORTED);
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
            if (!c->spec().caps.x_ticks) {
                return fail(err, "this chart kind has no explicit x-ticks", TC_ERR_UNSUPPORTED);
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
