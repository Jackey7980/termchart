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

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
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
#include "termchart/autoviz/engine.hpp"
#include "termchart/data/dataframe.hpp"
#include "termchart/api/quick.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/decoder.hpp"
#include "termchart/media/image/encode.hpp"
#include "termchart/util/file.hpp"
#include "termchart/backend/vector/backend.hpp"
#include "termchart/backend/vector/svg_backend.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/area.hpp"
#include "termchart/chart/cartesian/bar_chart.hpp"
#include "termchart/chart/cartesian/boxplot.hpp"
#include "termchart/chart/cartesian/bubble.hpp"
#include "termchart/chart/cartesian/bullet.hpp"
#include "termchart/chart/cartesian/candlestick.hpp"
#include "termchart/chart/cartesian/contour.hpp"
#include "termchart/chart/cartesian/dumbbell.hpp"
#include "termchart/chart/cartesian/errorbar.hpp"
#include "termchart/chart/cartesian/gantt.hpp"
#include "termchart/chart/cartesian/heatmap.hpp"
#include "termchart/chart/cartesian/hist.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/chart/cartesian/mosaic.hpp"
#include "termchart/chart/cartesian/parallel.hpp"
#include "termchart/chart/cartesian/qq.hpp"
#include "termchart/chart/cartesian/ridgeline.hpp"
#include "termchart/chart/cartesian/scatter_chart.hpp"
#include "termchart/chart/cartesian/sparkline.hpp"
#include "termchart/chart/cartesian/step.hpp"
#include "termchart/chart/cartesian/streamgraph.hpp"
#include "termchart/chart/cartesian/violin.hpp"
#include "termchart/chart/cartesian/waterfall.hpp"
#include "termchart/chart/context.hpp"
#include "termchart/chart/financial/depth.hpp"
#include "termchart/chart/financial/drawdown.hpp"
#include "termchart/chart/financial/kagi.hpp"
#include "termchart/chart/financial/point_figure.hpp"
#include "termchart/chart/financial/renko.hpp"
#include "termchart/chart/financial/volume.hpp"
#include "termchart/chart/financial/yield_curve.hpp"
#include "termchart/chart/flow/funnel.hpp"
#include "termchart/chart/flow/stream.hpp"
#include "termchart/chart/geo/choropleth.hpp"
#include "termchart/chart/geo/dot_density.hpp"
#include "termchart/chart/geo/geo_map.hpp"
#include "termchart/chart/geo/hexbin.hpp"
#include "termchart/chart/geo/tile_grid.hpp"
#include "termchart/chart/hierarchical/dendrogram.hpp"
#include "termchart/chart/hierarchical/icicle.hpp"
#include "termchart/chart/hierarchical/node.hpp"
#include "termchart/chart/hierarchical/pack.hpp"
#include "termchart/chart/hierarchical/sunburst.hpp"
#include "termchart/chart/hierarchical/tree_view.hpp"
#include "termchart/chart/hierarchical/treemap.hpp"
#include "termchart/chart/indicator/gauge_kpi.hpp"
#include "termchart/chart/indicator/progress_ring.hpp"
#include "termchart/chart/indicator/star_rating.hpp"
#include "termchart/chart/indicator/status_light.hpp"
#include "termchart/chart/indicator/thermometer.hpp"
#include "termchart/chart/polar/donut.hpp"
#include "termchart/chart/polar/gauge.hpp"
#include "termchart/chart/polar/pie.hpp"
#include "termchart/chart/polar/polar_bar.hpp"
#include "termchart/chart/polar/polar_heat.hpp"
#include "termchart/chart/polar/polar_line.hpp"
#include "termchart/chart/polar/radar.hpp"
#include "termchart/chart/polar/rose.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/chart/statistical/autocorr.hpp"
#include "termchart/chart/statistical/bland_altman.hpp"
#include "termchart/chart/statistical/corr.hpp"
#include "termchart/chart/statistical/distribution.hpp"
#include "termchart/chart/statistical/ecdf.hpp"
#include "termchart/chart/statistical/forest.hpp"
#include "termchart/chart/statistical/pca.hpp"
#include "termchart/chart/statistical/regression.hpp"
#include "termchart/chart/statistical/roc.hpp"
#include "termchart/chart/statistical/seasonal.hpp"
#include "termchart/chart/statistical/survival.hpp"
#include "termchart/chart/text/progress.hpp"
#include "termchart/chart/text/table.hpp"
#include "termchart/chart/text/tag_cloud.hpp"
#include "termchart/chart/text/wordcloud.hpp"
#include "termchart/core/color_depth.hpp"
#include "termchart/core/error.hpp"
#include "termchart/core/style.hpp"
#include "termchart/gui/cells.hpp"
#include "termchart/media/image/encode.hpp"
#include "termchart/render/compositor.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"
#include "termchart/version.hpp"

#include "termchart/media/media.hpp"

// 关系图 / 文档 / 终端协议：C 面直接消费与 C++ 同一批入口，不另建绘制逻辑。
#include "termchart/backend/structured/html_backend.hpp"
#include "termchart/backend/structured/markdown_backend.hpp"
#include "termchart/document/block.hpp"
#include "termchart/document/document.hpp"
#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/dsl_registry.hpp"
#include "termchart/graph/diagrams/dag.hpp"
#include "termchart/graph/diagrams/dependency.hpp"
#include "termchart/graph/diagrams/er_diagram.hpp"
#include "termchart/graph/diagrams/flowchart.hpp"
#include "termchart/graph/diagrams/knowledge_graph.hpp"
#include "termchart/graph/diagrams/mindmap.hpp"
#include "termchart/graph/diagrams/orgchart.hpp"
#include "termchart/graph/diagrams/social_network.hpp"
#include "termchart/graph/diagrams/state_machine.hpp"
#include "termchart/graph/diagrams/uml_activity.hpp"
#include "termchart/graph/diagrams/uml_class.hpp"
#include "termchart/graph/diagrams/uml_usecase.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/protocol/detect.hpp"
#include "termchart/protocol/fallback.hpp"
#include "termchart/protocol/halfblock_image.hpp"
#include "termchart/protocol/iterm2_inline.hpp"
#include "termchart/protocol/kitty_graphics.hpp"
#include "termchart/protocol/sixel.hpp"

// 终端 / 输入 / 命中 / 导出物种 / 时序图 / 多帧编码 / 3D·4D。
#include "termchart/backend/snapshot/backend.hpp"
#include "termchart/backend/snapshot/html_pre_backend.hpp"
#include "termchart/backend/snapshot/json_backend.hpp"
#include "termchart/backend/snapshot/png_screenshot.hpp"
#include "termchart/backend/snapshot/text_backend.hpp"
#include "termchart/backend/vector/eps_backend.hpp"
#include "termchart/backend/vector/latex_backend.hpp"
#include "termchart/backend/vector/pdf_backend.hpp"
#include "termchart/canvas/scene_renderer.hpp"
#include "termchart/chart/hit_region.hpp"
#include "termchart/chart3d/projector.hpp"
#include "termchart/chart3d/scatter.hpp"
#include "termchart/chart3d/surface.hpp"
#include "termchart/chart4d/projector4d.hpp"
#include "termchart/chart4d/wire.hpp"
#include "termchart/document/span.hpp"
#include "termchart/dsl/mermaid_sequence.hpp"
#include "termchart/graph/diagrams/uml_sequence.hpp"
#include "termchart/interaction/hit_test.hpp"
#include "termchart/interaction/input_parser.hpp"
#include "termchart/math3d/camera.hpp"
#include "termchart/math4d/camera4.hpp"
#include "termchart/math4d/mat5.hpp"
#include "termchart/term/caps.hpp"
#include "termchart/term/clipboard.hpp"
#include "termchart/term/hyperlink.hpp"
#include "termchart/term/mouse_mode.hpp"
#include "termchart/term/size.hpp"
#include "termchart/term/terminal.hpp"

using namespace termchart;

namespace {

struct ChartData;  // 前向声明：ChartSpec::apply 与 ChartData::spec 互相引用

// 数据形状：决定哪个数据入口能灌、has_data 看哪个容器。加图种时只需在 spec 表里
// 声明形状，不必再改 add_series / set_values / set_matrix / has_data 的 if-else。
enum class DataShape {
    Series,   // 多系列（line / scatter / streamgraph / stream）
    Values,   // 一组 double + 类别标签（bar / pie / hist / wordcloud …）
    Points,   // 一组 (x,y)（area / step / heatmap / polar_line …）
    Records,  // 行式 double 元组 + 可选 labels：专属 struct 图种按列语义解释
              // （candlestick/bubble/errorbar/bullet/…/contour/corr/… 共用 set_matrix 入口）
    Scalar,   // 单 double（gauge / progress_ring / star_rating / thermometer / status_light）
    Tree,     // 扁平 parent/name/value（treemap/sunburst/icicle/dendrogram/pack/tree_view）
    Text,     // 字符串：table（矩阵）/ gauge_kpi / geo_map(geojson) / region(cells)
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
    std::vector<std::vector<double>> matrix;  // Records 形状：行式 double 元组
    std::vector<std::string> row_labels;      // Records 的可选行/列标签
    std::vector<std::string> col_labels;

    double scalar = 0.0;                      // Scalar 形状：单值
    // Tree 形状：扁平节点（parent 索引，-1 为根；name；value）
    struct FlatNode { int parent; std::string name; double value; };
    std::vector<FlatNode> tree;
    // Text 形状：字符串矩阵（table / gauge_kpi / geojson / region 依附数据）
    std::vector<std::vector<std::string>> text_rows;
    std::string text_blob;                    // 单串（geojson / gauge_kpi 主值）
    // Region 形状：name + cells(x,y) + value（choropleth / dot_density / geo_map）
    struct Region { std::string name; std::vector<Point> cells; double value = 0.0; };
    std::vector<Region> regions;

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
// 一批图种用 set_points(vector<Point>) / set_prices(vector<double>)，与通用 set_values 不同名。
template <class T>
void apply_set_points(chart::Chart& ch, const ChartData& d) {
    std::vector<Point> pts;
    if (!d.series.empty()) pts = d.series.front().points;
    static_cast<T&>(ch).set_points(std::move(pts));
}
template <class T>
void apply_set_prices(chart::Chart& ch, const ChartData& d) {
    static_cast<T&>(ch).set_prices(d.values);
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

// ── Records 形状：专属 struct 图种，按列语义把行式 double 转成结构 ──────────
inline std::string lbl(const ChartData& d, std::size_t i) {
    return i < d.row_labels.size() ? d.row_labels[i] : std::string{};
}
inline double cell(const ChartData& d, std::size_t r, std::size_t c) {
    return (r < d.matrix.size() && c < d.matrix[r].size()) ? d.matrix[r][c] : 0.0;
}
inline void apply_candlestick(chart::Chart& ch, const ChartData& d) {
    std::vector<chart::CandlestickChart::Ohlc> v;
    for (std::size_t i = 0; i < d.matrix.size(); ++i)
        v.push_back({cell(d,i,0), cell(d,i,1), cell(d,i,2), cell(d,i,3)});
    static_cast<chart::CandlestickChart&>(ch).set_data(std::move(v));
}
inline void apply_bubble(chart::Chart& ch, const ChartData& d) {
    std::vector<chart::BubbleChart::Bubble> v;
    for (std::size_t i = 0; i < d.matrix.size(); ++i)
        v.push_back({Point{cell(d,i,0), cell(d,i,1)}, cell(d,i,2)});
    static_cast<chart::BubbleChart&>(ch).set_data(std::move(v));
}
inline void apply_errorbar(chart::Chart& ch, const ChartData& d) {
    std::vector<chart::ErrorBarChart::ErrorPoint> v;
    for (std::size_t i = 0; i < d.matrix.size(); ++i)
        v.push_back({cell(d,i,0), cell(d,i,1), cell(d,i,2), cell(d,i,3), cell(d,i,4), cell(d,i,5)});
    static_cast<chart::ErrorBarChart&>(ch).set_data(std::move(v));
}
inline void apply_boxplot(chart::Chart& ch, const ChartData& d) {
    std::vector<chart::BoxPlotChart::BoxStats> v;
    std::vector<std::string> labels;
    for (std::size_t i = 0; i < d.matrix.size(); ++i) {
        chart::BoxPlotChart::BoxStats b{cell(d,i,0), cell(d,i,1), cell(d,i,2),
                                        cell(d,i,3), cell(d,i,4), {}};
        for (std::size_t c = 5; c < d.matrix[i].size(); ++c) b.outliers.push_back(d.matrix[i][c]);
        v.push_back(std::move(b));
        labels.push_back(lbl(d, i));
    }
    static_cast<chart::BoxPlotChart&>(ch).set_data(std::move(v));
    static_cast<chart::BoxPlotChart&>(ch).set_labels(std::move(labels));
}
inline void apply_bullet(chart::Chart& ch, const ChartData& d) {
    std::vector<chart::BulletChart::Row> v;
    for (std::size_t i = 0; i < d.matrix.size(); ++i)
        v.push_back({lbl(d,i), cell(d,i,0), cell(d,i,1),
                     {cell(d,i,2), cell(d,i,3), cell(d,i,4)}});
    static_cast<chart::BulletChart&>(ch).set_rows(std::move(v));
}
inline void apply_dumbbell(chart::Chart& ch, const ChartData& d) {
    std::vector<chart::DumbbellChart::Row> v;
    for (std::size_t i = 0; i < d.matrix.size(); ++i)
        v.push_back({lbl(d,i), cell(d,i,0), cell(d,i,1)});
    static_cast<chart::DumbbellChart&>(ch).set_rows(std::move(v));
}
inline void apply_gantt(chart::Chart& ch, const ChartData& d) {
    std::vector<chart::GanttChart::Task> v;
    for (std::size_t i = 0; i < d.matrix.size(); ++i)
        v.push_back({lbl(d,i), cell(d,i,0), cell(d,i,1)});
    static_cast<chart::GanttChart&>(ch).set_data(std::move(v));
}
inline void apply_waterfall(chart::Chart& ch, const ChartData& d) {
    std::vector<chart::WaterfallChart::Item> v;
    for (std::size_t i = 0; i < d.matrix.size(); ++i)
        v.push_back({lbl(d,i), cell(d,i,0), cell(d,i,1) != 0.0});
    static_cast<chart::WaterfallChart&>(ch).set_data(std::move(v));
}
inline void apply_forest(chart::Chart& ch, const ChartData& d) {
    std::vector<chart::ForestStudy> v;
    for (std::size_t i = 0; i < d.matrix.size(); ++i)
        v.push_back({lbl(d,i), cell(d,i,0), cell(d,i,1), cell(d,i,2)});
    static_cast<chart::ForestChart&>(ch).set_studies(std::move(v));
}
inline void apply_funnel(chart::Chart& ch, const ChartData& d) {
    std::vector<chart::FunnelStage> v;
    for (std::size_t i = 0; i < d.matrix.size(); ++i)
        v.push_back({lbl(d,i), cell(d,i,0), d.bar_color});
    static_cast<chart::FunnelChart&>(ch).set_stages(std::move(v));
}
inline void apply_yield_curve(chart::Chart& ch, const ChartData& d) {
    std::vector<double> m, y;
    for (std::size_t i = 0; i < d.matrix.size(); ++i) {
        m.push_back(cell(d,i,0)); y.push_back(cell(d,i,1));
    }
    static_cast<chart::YieldCurveChart&>(ch).set_curve(std::move(m), std::move(y),
                                                       d.bar_color, d.bar_name);
}
inline void apply_bland_altman(chart::Chart& ch, const ChartData& d) {
    std::vector<double> a, b;
    for (std::size_t i = 0; i < d.matrix.size(); ++i) {
        a.push_back(cell(d,i,0)); b.push_back(cell(d,i,1));
    }
    static_cast<chart::BlandAltmanChart&>(ch).set_pairs(std::move(a), std::move(b),
                                                        d.bar_color, d.bar_name);
}
inline void apply_roc(chart::Chart& ch, const ChartData& d) {
    std::vector<double> scores; std::vector<int> labels;
    for (std::size_t i = 0; i < d.matrix.size(); ++i) {
        scores.push_back(cell(d,i,0)); labels.push_back(static_cast<int>(cell(d,i,1)));
    }
    static_cast<chart::RocChart&>(ch).set_scores(std::move(scores), std::move(labels), d.bar_name);
}
inline void apply_survival(chart::Chart& ch, const ChartData& d) {
    std::vector<double> times; std::vector<int> events;
    for (std::size_t i = 0; i < d.matrix.size(); ++i) {
        times.push_back(cell(d,i,0)); events.push_back(static_cast<int>(cell(d,i,1)));
    }
    static_cast<chart::SurvivalChart&>(ch).set_data(std::move(times), std::move(events),
                                                    d.bar_color, d.bar_name);
}
inline void apply_volume(chart::Chart& ch, const ChartData& d) {
    std::vector<double> values; std::vector<int> dirs;
    for (std::size_t i = 0; i < d.matrix.size(); ++i) {
        values.push_back(cell(d,i,0));
        if (d.matrix[i].size() > 1) dirs.push_back(static_cast<int>(cell(d,i,1)));
    }
    static_cast<chart::VolumeChart&>(ch).set_values(std::move(values), std::move(dirs));
}
inline void apply_depth(chart::Chart& ch, const ChartData& d) {
    // 行按 [bid_price, bid_qty, ask_price, ask_qty]；长度不足的列为 0
    std::vector<double> bp, bq, ap, aq;
    for (std::size_t i = 0; i < d.matrix.size(); ++i) {
        if (cell(d,i,0) != 0.0 || cell(d,i,1) != 0.0) { bp.push_back(cell(d,i,0)); bq.push_back(cell(d,i,1)); }
        if (cell(d,i,2) != 0.0 || cell(d,i,3) != 0.0) { ap.push_back(cell(d,i,2)); aq.push_back(cell(d,i,3)); }
    }
    static_cast<chart::DepthChart&>(ch).set_book(std::move(bp), std::move(bq),
                                                 std::move(ap), std::move(aq));
}
inline void apply_tile_grid(chart::Chart& ch, const ChartData& d) {
    auto& g = static_cast<chart::TileGrid&>(ch);
    for (std::size_t i = 0; i < d.matrix.size(); ++i)
        g.add_tile({lbl(d,i), static_cast<int>(cell(d,i,0)),
                    static_cast<int>(cell(d,i,1)), cell(d,i,2)});
}
inline void apply_hexbin(chart::Chart& ch, const ChartData& d) {
    auto& h = static_cast<chart::Hexbin&>(ch);
    for (std::size_t i = 0; i < d.matrix.size(); ++i) h.add_point(cell(d,i,0), cell(d,i,1));
}
inline void apply_ridgeline(chart::Chart& ch, const ChartData& d) {
    // 每行 = 一个 group 的变长 values（行标签作 group 名）
    std::vector<chart::RidgelineChart::Group> v;
    for (std::size_t i = 0; i < d.matrix.size(); ++i)
        v.push_back({lbl(d,i), d.matrix[i], d.bar_color});
    static_cast<chart::RidgelineChart&>(ch).set_groups(std::move(v));
}

// ── Values 逐条 add_* 的图种（词云 / 标签云 / 进度条）─────────────────────
inline void apply_wordcloud(chart::Chart& ch, const ChartData& d) {
    auto& w = static_cast<chart::Wordcloud&>(ch);
    for (std::size_t i = 0; i < d.values.size(); ++i) w.add_word(lbl(d,i), d.values[i]);
}
inline void apply_tag_cloud(chart::Chart& ch, const ChartData& d) {
    auto& w = static_cast<chart::TagCloud&>(ch);
    for (std::size_t i = 0; i < d.values.size(); ++i) w.add_tag(lbl(d,i), d.values[i]);
}
inline void apply_progress(chart::Chart& ch, const ChartData& d) {
    auto& w = static_cast<chart::Progress&>(ch);
    for (std::size_t i = 0; i < d.values.size(); ++i) w.add_bar(lbl(d,i), d.values[i], d.bar_color);
}

// ── Scalar 单值图种 ─────────────────────────────────────────────────────────
inline void apply_gauge(chart::Chart& ch, const ChartData& d) {
    static_cast<chart::GaugeChart&>(ch).set_value(d.scalar);
}
inline void apply_progress_ring(chart::Chart& ch, const ChartData& d) {
    static_cast<chart::ProgressRing&>(ch).set_fraction(d.scalar);
}
inline void apply_star_rating(chart::Chart& ch, const ChartData& d) {
    static_cast<chart::StarRating&>(ch).set_rating(d.scalar);
}
inline void apply_thermometer(chart::Chart& ch, const ChartData& d) {
    static_cast<chart::Thermometer&>(ch).set_value(d.scalar);
}
inline void apply_status_light(chart::Chart& ch, const ChartData& d) {
    auto st = static_cast<chart::LightState>(static_cast<int>(d.scalar));
    static_cast<chart::StatusLight&>(ch).set_state(st);
}

// ── Tree 层次图：从扁平 parent/name/value 重建 HierNode ────────────────────
inline chart::hier::HierNode build_tree(const ChartData& d) {
    // 约定：行号即节点索引，parent=-1 为根；支持空表（返回空根）
    if (d.tree.empty()) return {};
    std::vector<chart::hier::HierNode> nodes(d.tree.size());
    std::vector<int> parent(d.tree.size());
    chart::hier::HierNode* root = nullptr;
    for (std::size_t i = 0; i < d.tree.size(); ++i) {
        nodes[i].name = d.tree[i].name;
        nodes[i].value = d.tree[i].value;
        parent[i] = d.tree[i].parent;
    }
    for (std::size_t i = 0; i < d.tree.size(); ++i) {
        if (parent[i] < 0 || static_cast<std::size_t>(parent[i]) >= nodes.size()) {
            root = &nodes[i];
        } else {
            nodes[parent[i]].children.push_back(nodes[i]);
        }
    }
    return root ? *root : nodes.front();
}
template <class T>
void apply_tree(chart::Chart& ch, const ChartData& d) {
    static_cast<T&>(ch).set_root(build_tree(d));
}

// ── Text 图种 ───────────────────────────────────────────────────────────────
inline void apply_table(chart::Chart& ch, const ChartData& d) {
    auto& t = static_cast<chart::Table&>(ch);
    // 首行为列名，其余为数据行
    if (!d.text_rows.empty()) {
        for (const auto& name : d.text_rows.front()) t.add_column(name);
        for (std::size_t r = 1; r < d.text_rows.size(); ++r) t.add_row(d.text_rows[r]);
    }
}
inline void apply_gauge_kpi(chart::Chart& ch, const ChartData& d) {
    auto& g = static_cast<chart::GaugeKpi&>(ch);
    // text_rows: [[label, value]] 或 blob 作 value
    if (d.text_rows.size() >= 1 && d.text_rows[0].size() >= 1) g.set_label(d.text_rows[0][0]);
    if (d.text_rows.size() >= 1 && d.text_rows[0].size() >= 2) g.set_value(d.text_rows[0][1]);
    else if (!d.text_blob.empty()) g.set_value(d.text_blob);
    if (d.text_rows.size() >= 2 && d.text_rows[1].size() >= 1) g.set_delta(d.text_rows[1][0], false);
}

// ── Region 地理图种（name + cells + value）─────────────────────────────────
inline void apply_choropleth(chart::Chart& ch, const ChartData& d) {
    auto& g = static_cast<chart::Choropleth&>(ch);
    for (const auto& r : d.regions) g.add_region({r.name, r.cells, r.value});
}
inline void apply_dot_density(chart::Chart& ch, const ChartData& d) {
    auto& g = static_cast<chart::DotDensity&>(ch);
    for (const auto& r : d.regions) g.add_region({r.name, r.cells, r.value, d.bar_color});
}
inline void apply_geo_map(chart::Chart& ch, const ChartData& d) {
    auto& g = static_cast<chart::GeoMap&>(ch);
    if (!d.text_blob.empty()) { std::string err; g.load_geojson(d.text_blob, err); }
    for (const auto& r : d.regions) g.set_value(r.name, r.value);
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
    {"contour",    make_chart<chart::ContourChart>,   apply_contour,  DataShape::Records, Capabilities{}, nullptr},
    {"corr",       make_chart<chart::CorrMatrixChart>,apply_corr,     DataShape::Records, Capabilities{}, nullptr},
    {"mosaic",     make_chart<chart::MosaicChart>,    apply_mosaic,   DataShape::Records, Capabilities{}, nullptr},
    {"violin",     make_chart<chart::ViolinChart>,    apply_violin,   DataShape::Records, Capabilities{}, nullptr},
    {"pca",        make_chart<chart::PcaChart>,       apply_pca,      DataShape::Records, Capabilities{}, nullptr},
    {"parallel",   make_chart<chart::ParallelChart>,  apply_parallel, DataShape::Records, Capabilities{}, nullptr},

    // ── Records（行式 double 按列语义解释，入口 tc_chart_set_matrix）──
    {"candlestick", make_chart<chart::CandlestickChart>, apply_candlestick,  DataShape::Records, Capabilities{}, nullptr},
    {"bubble",      make_chart<chart::BubbleChart>,      apply_bubble,       DataShape::Records, Capabilities{}, nullptr},
    {"errorbar",    make_chart<chart::ErrorBarChart>,    apply_errorbar,     DataShape::Records, Capabilities{}, nullptr},
    {"boxplot",     make_chart<chart::BoxPlotChart>,     apply_boxplot,      DataShape::Records, Capabilities{}, nullptr},
    {"bullet",      make_chart<chart::BulletChart>,      apply_bullet,       DataShape::Records, Capabilities{}, nullptr},
    {"dumbbell",    make_chart<chart::DumbbellChart>,    apply_dumbbell,     DataShape::Records, Capabilities{}, nullptr},
    {"gantt",       make_chart<chart::GanttChart>,       apply_gantt,        DataShape::Records, Capabilities{}, nullptr},
    {"waterfall",   make_chart<chart::WaterfallChart>,   apply_waterfall,    DataShape::Records, Capabilities{}, nullptr},
    {"forest",      make_chart<chart::ForestChart>,      apply_forest,       DataShape::Records, Capabilities{}, nullptr},
    {"funnel",      make_chart<chart::FunnelChart>,      apply_funnel,       DataShape::Records, Capabilities{}, nullptr},
    {"yield_curve", make_chart<chart::YieldCurveChart>,  apply_yield_curve,  DataShape::Records, Capabilities{}, nullptr},
    {"bland_altman",make_chart<chart::BlandAltmanChart>, apply_bland_altman, DataShape::Records, Capabilities{}, nullptr},
    {"roc",         make_chart<chart::RocChart>,         apply_roc,          DataShape::Records, Capabilities{}, nullptr},
    {"survival",    make_chart<chart::SurvivalChart>,    apply_survival,     DataShape::Records, Capabilities{}, nullptr},
    {"volume",      make_chart<chart::VolumeChart>,      apply_volume,       DataShape::Records, Capabilities{}, nullptr},
    {"depth",       make_chart<chart::DepthChart>,       apply_depth,        DataShape::Records, Capabilities{}, nullptr},
    {"tile_grid",   make_chart<chart::TileGrid>,         apply_tile_grid,    DataShape::Records, Capabilities{}, nullptr},
    {"hexbin",      make_chart<chart::Hexbin>,           apply_hexbin,       DataShape::Records, Capabilities{}, nullptr},
    {"ridgeline",   make_chart<chart::RidgelineChart>,   apply_ridgeline,    DataShape::Records, Capabilities{}, nullptr},

    // ── Points（极地）──
    {"polar_line",  make_chart<chart::PolarLineChart>,  apply_set_points<chart::PolarLineChart>,  DataShape::Points, Capabilities{}, nullptr},
    {"polar_heat",  make_chart<chart::PolarHeatChart>,  apply_set_points<chart::PolarHeatChart>,  DataShape::Points, Capabilities{}, nullptr},

    // ── Values（逐条 add_* / set_prices）──
    {"wordcloud",    make_chart<chart::Wordcloud>,        apply_wordcloud,   DataShape::Values, Capabilities{}, nullptr},
    {"tag_cloud",    make_chart<chart::TagCloud>,         apply_tag_cloud,   DataShape::Values, Capabilities{}, nullptr},
    {"progress",     make_chart<chart::Progress>,         apply_progress,    DataShape::Values, Capabilities{}, nullptr},
    {"drawdown",     make_chart<chart::DrawdownChart>,    apply_set_prices<chart::DrawdownChart>,     DataShape::Values, Capabilities{}, nullptr},
    {"kagi",         make_chart<chart::KagiChart>,        apply_set_prices<chart::KagiChart>,         DataShape::Values, Capabilities{}, nullptr},
    {"point_figure", make_chart<chart::PointFigureChart>, apply_set_prices<chart::PointFigureChart>,  DataShape::Values, Capabilities{}, nullptr},
    {"renko",        make_chart<chart::RenkoChart>,       apply_set_prices<chart::RenkoChart>,        DataShape::Values, Capabilities{}, nullptr},

    // ── Scalar（入口 tc_chart_set_scalar）──
    {"gauge",         make_chart<chart::GaugeChart>,     apply_gauge,         DataShape::Scalar, Capabilities{}, nullptr},
    {"progress_ring", make_chart<chart::ProgressRing>,   apply_progress_ring, DataShape::Scalar, Capabilities{}, nullptr},
    {"star_rating",   make_chart<chart::StarRating>,     apply_star_rating,   DataShape::Scalar, Capabilities{}, nullptr},
    {"thermometer",   make_chart<chart::Thermometer>,    apply_thermometer,   DataShape::Scalar, Capabilities{}, nullptr},
    {"status_light",  make_chart<chart::StatusLight>,    apply_status_light,  DataShape::Scalar, Capabilities{}, nullptr},

    // ── Tree（入口 tc_chart_set_tree）──
    {"treemap",    make_chart<chart::Treemap>,    apply_tree<chart::Treemap>,    DataShape::Tree, Capabilities{}, nullptr},
    {"sunburst",   make_chart<chart::Sunburst>,   apply_tree<chart::Sunburst>,   DataShape::Tree, Capabilities{}, nullptr},
    {"icicle",     make_chart<chart::Icicle>,     apply_tree<chart::Icicle>,     DataShape::Tree, Capabilities{}, nullptr},
    {"dendrogram", make_chart<chart::Dendrogram>, apply_tree<chart::Dendrogram>, DataShape::Tree, Capabilities{}, nullptr},
    {"pack",       make_chart<chart::PackChart>,  apply_tree<chart::PackChart>,  DataShape::Tree, Capabilities{}, nullptr},
    {"tree_view",  make_chart<chart::TreeView>,   apply_tree<chart::TreeView>,   DataShape::Tree, Capabilities{}, nullptr},

    // ── Text（入口 tc_chart_set_text）──
    {"table",     make_chart<chart::Table>,    apply_table,     DataShape::Text, Capabilities{}, nullptr},
    {"gauge_kpi", make_chart<chart::GaugeKpi>, apply_gauge_kpi, DataShape::Text, Capabilities{}, nullptr},

    // ── Region（入口 tc_chart_set_region；geo_map 另可用 text_blob 灌 GeoJSON）──
    {"choropleth",  make_chart<chart::Choropleth>,  apply_choropleth,  DataShape::Text, Capabilities{}, nullptr},
    {"dot_density", make_chart<chart::DotDensity>,  apply_dot_density, DataShape::Text, Capabilities{}, nullptr},
    {"geo_map",     make_chart<chart::GeoMap>,      apply_geo_map,     DataShape::Text, Capabilities{}, nullptr},
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

// ErrorCode → tc_status 的**唯一映射**（fail-loud：未知码不静默降级成通用 RENDER）。
// 与 termchart_capi.h 的状态码只增不改契约对齐；C++ 侧新增 ErrorCode 时在此补分支。
inline tc_status status_to_tc(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Ok:
            return TC_OK;
        case ErrorCode::InvalidArgument:
        case ErrorCode::OutOfRange:
        case ErrorCode::MalformedUtf8:
        case ErrorCode::ParseError:
            return TC_ERR_INVALID_ARG;
        case ErrorCode::Unsupported:
        case ErrorCode::NotImplemented:
            return TC_ERR_UNSUPPORTED;
        case ErrorCode::NotInitialized:
            return TC_ERR_INVALID_HANDLE;
        case ErrorCode::IoError:
        case ErrorCode::TerminalUnavailable:
        case ErrorCode::PlatformError:
            return TC_ERR_IO;
    }
    return TC_ERR_RENDER;  // 未知/新增码：显式归 RENDER，不伪装成 OK
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
        case DataShape::Records: return !d.matrix.empty();
        case DataShape::Series:
        case DataShape::Points:
            for (const auto& s : d.series) {
                if (!s.points.empty()) return true;
            }
            return false;
        case DataShape::Scalar: return true;  // 单值总有效（含 0）
        case DataShape::Tree:   return !d.tree.empty();
        case DataShape::Text:   return !d.text_rows.empty() || !d.text_blob.empty() ||
                                       !d.regions.empty();
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
    if (!text || !out_ansi) return fail(err, "null text or output buffer", TC_ERR_INVALID_ARG);
    const Style st = to_style(style);
    const api::ApiOptions o = to_api_opts(opts);
    auto r = api::render_line(std::string_view(text), st, o);
    if (!r.has_value()) {
        return fail(err, std::string("render_line failed (") + describe(r.error()) + ")",
                    status_to_tc(r.error()));
    }
    return store_string(r.value(), out_ansi, err, 4);
}

tc_status tc_print_line(const char* text, const tc_style* style, const tc_api_opts* opts,
                        tc_error_handle* err) {
    if (!text) return fail(err, "null text", TC_ERR_INVALID_ARG);
    const Status s = api::print_line(std::string_view(text), to_style(style), to_api_opts(opts));
    if (!is_ok(s)) {
        return fail(err, std::string("print_line failed (") + describe(s) + ")", status_to_tc(s));
    }
    return 0;
}

tc_status tc_hello(const tc_api_opts* opts, tc_error_handle* err) {
    const Status s = api::hello(to_api_opts(opts));
    if (!is_ok(s)) {
        return fail(err, std::string("hello failed (") + describe(s) + ")", status_to_tc(s));
    }
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
    if (c->spec().shape != DataShape::Records) {
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

// 单值图种（gauge / progress_ring / star_rating / thermometer / status_light）。
tc_status tc_chart_set_scalar(tc_chart_handle chart, double value, tc_error_handle* err) {
    if (!chart) return fail(err, "null chart", TC_ERR_INVALID_HANDLE);
    auto* c = reinterpret_cast<ChartData*>(chart);
    if (c->spec().shape != DataShape::Scalar) {
        return fail(err, "this chart kind does not take a scalar", TC_ERR_UNSUPPORTED);
    }
    c->scalar = value;
    c->invalidate();
    return 0;
}

// 层次图（treemap/sunburst/icicle/dendrogram/pack/tree_view）。
// parents[i] = 父节点索引（-1 为根）；names[i]/values[i] 为节点名与值。
tc_status tc_chart_set_tree(tc_chart_handle chart, const int* parents,
                            const char* const* names, const double* values, uint64_t n,
                            tc_error_handle* err) {
    if (!chart) return fail(err, "null chart", TC_ERR_INVALID_HANDLE);
    if (!parents || !names || !values) return fail(err, "null tree arrays", TC_ERR_INVALID_ARG);
    auto* c = reinterpret_cast<ChartData*>(chart);
    if (c->spec().shape != DataShape::Tree) {
        return fail(err, "this chart kind does not take a tree", TC_ERR_UNSUPPORTED);
    }
    c->tree.clear();
    c->tree.reserve(static_cast<std::size_t>(n));
    for (uint64_t i = 0; i < n; ++i)
        c->tree.push_back({parents[i], names[i] ? names[i] : "", values[i]});
    c->invalidate();
    return 0;
}

// 文本图种：table（rows 为 [[列名...],[行...]]）/ gauge_kpi（[[label,value],[delta]]）/
// geo_map（blob 为 GeoJSON 文本）。blob 可为 NULL。
tc_status tc_chart_set_text(tc_chart_handle chart, const char* const* const* rows,
                            uint64_t row_count, const uint64_t* col_counts,
                            const char* blob, tc_error_handle* err) {
    if (!chart) return fail(err, "null chart", TC_ERR_INVALID_HANDLE);
    auto* c = reinterpret_cast<ChartData*>(chart);
    if (c->spec().shape != DataShape::Text) {
        return fail(err, "this chart kind does not take text", TC_ERR_UNSUPPORTED);
    }
    c->text_rows.clear();
    for (uint64_t i = 0; i < row_count; ++i) {
        const std::size_t m = col_counts ? static_cast<std::size_t>(col_counts[i]) : 0;
        std::vector<std::string> row;
        for (std::size_t j = 0; j < m; ++j)
            row.emplace_back(rows[i][j] ? rows[i][j] : "");
        c->text_rows.push_back(std::move(row));
    }
    c->text_blob = blob ? blob : "";
    c->invalidate();
    return 0;
}

// 区域地理图种（choropleth / dot_density / geo_map 的数值覆盖）。
// 每个区域：name + n_cells 个 (x,y) + value。分两次调用不方便，故用扁平数组：
// cell_x/cell_y/cell_count 给出所有格，region_starts 给出每个区域在格数组中的起点。
tc_status tc_chart_set_regions(tc_chart_handle chart,
                               const char* const* names, const double* values, uint64_t n_regions,
                               const double* cell_x, const double* cell_y,
                               const uint64_t* cell_starts, const uint64_t* cell_counts,
                               uint64_t n_cells, tc_error_handle* err) {
    if (!chart) return fail(err, "null chart", TC_ERR_INVALID_HANDLE);
    if (!names || !values) return fail(err, "null region arrays", TC_ERR_INVALID_ARG);
    auto* c = reinterpret_cast<ChartData*>(chart);
    if (c->spec().shape != DataShape::Text) {
        return fail(err, "this chart kind does not take regions", TC_ERR_UNSUPPORTED);
    }
    c->regions.clear();
    c->regions.reserve(static_cast<std::size_t>(n_regions));
    for (uint64_t i = 0; i < n_regions; ++i) {
        ChartData::Region r;
        r.name = names[i] ? names[i] : "";
        r.value = values[i];
        if (cell_starts && cell_counts) {
            const uint64_t s = cell_starts[i], cnt = cell_counts[i];
            if (cell_x && cell_y && s + cnt <= n_cells)
                for (uint64_t k = s; k < s + cnt; ++k)
                    r.cells.push_back({cell_x[k], cell_y[k]});
        }
        c->regions.push_back(std::move(r));
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
    auto sc = api::chart_to_scene(ch, w, h);
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
    if (!bytes && n) return fail(err, "null bytes", TC_ERR_INVALID_ARG);
    const Status s = api::write_stdout(sv_of(bytes, n));
    if (!is_ok(s)) {
        return fail(err, std::string("write_stdout failed (") + describe(s) + ")", status_to_tc(s));
    }
    return 0;
}

tc_status tc_write_stderr(const char* bytes, uint64_t n, tc_error_handle* err) {
    if (!bytes && n) return fail(err, "null bytes", TC_ERR_INVALID_ARG);
    const Status s = api::write_stderr(sv_of(bytes, n));
    if (!is_ok(s)) {
        return fail(err, std::string("write_stderr failed (") + describe(s) + ")", status_to_tc(s));
    }
    return 0;
}

tc_status tc_flush_stdout(tc_error_handle* err) {
    const Status s = api::flush_stdout();
    if (!is_ok(s)) {
        return fail(err, std::string("flush_stdout failed (") + describe(s) + ")", status_to_tc(s));
    }
    return 0;
}

// ── AutoViz ─────────────────────────────────────────────────────────────────

tc_status tc_auto_plot(const char* filepath, tc_error_handle* err) {
    if (!filepath) return fail(err, "null filepath", TC_ERR_INVALID_ARG);
    const Status s = api::auto_plot(std::string_view(filepath));
    if (!is_ok(s)) {
        return fail(err, std::string("auto_plot failed (") + describe(s) + ")", status_to_tc(s));
    }
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

// ── 一行出图（直通 api::render_*，不另拼 Chart）──────────────────────────────

static api::PlotOpts quick_opts_of(const tc_plot_opts* o) {
    api::PlotOpts p{};
    if (o == nullptr) {
        return p;
    }
    if (o->width) p.width = o->width;
    if (o->height) p.height = o->height;
    if (o->title) p.title = o->title;
    if (o->depth != TC_DEPTH_NONE) p.depth = to_depth(o->depth);
    p.show_legend = o->show_legend != 0;
    return p;
}

static tc_status emit_result(const Result<std::string>& r, char* buf, std::uint64_t cap,
                             std::uint64_t* out_len, tc_error_handle* err) {
    if (!r.has_value()) {
        return fail(err, "quick render failed (code " +
                             std::to_string(static_cast<int>(r.error())) + ")",
                    status_to_tc(r.error()));
    }
    const std::string& text = r.value();
    if (out_len != nullptr) {
        *out_len = text.size();
    }
    if (buf == nullptr || cap < text.size()) {
        return TC_ERR_BUFFER_TOO_SMALL;
    }
    std::memcpy(buf, text.data(), text.size());
    return TC_OK;
}

tc_status tc_quick_xy(tc_quick_kind kind, const double* x, const double* y, std::uint64_t n,
                      const tc_plot_opts* opts, char* buf, std::uint64_t cap,
                      std::uint64_t* out_len, tc_error_handle* err) {
    if (x == nullptr || y == nullptr || n == 0) {
        return fail(err, "quick plot: empty x/y", TC_ERR_INVALID_ARG);
    }
    const std::vector<double> xs(x, x + n);
    const std::vector<double> ys(y, y + n);
    const api::PlotOpts o = quick_opts_of(opts);
    const auto r = (kind == TC_QUICK_SCATTER) ? api::render_scatter(xs, ys, o)
                                              : api::render_plot(xs, ys, o);
    return emit_result(r, buf, cap, out_len, err);
}

tc_status tc_quick_bar(const double* values, const char* const* labels, std::uint64_t n,
                       const tc_plot_opts* opts, char* buf, std::uint64_t cap,
                       std::uint64_t* out_len, tc_error_handle* err) {
    if (values == nullptr || n == 0) {
        return fail(err, "quick bar: empty values", TC_ERR_INVALID_ARG);
    }
    std::vector<double> vals(values, values + n);
    std::vector<std::string> cats;
    if (labels != nullptr) {
        cats.reserve(static_cast<std::size_t>(n));
        for (std::uint64_t i = 0; i < n; ++i) {
            cats.push_back(labels[i] == nullptr ? std::string{} : std::string(labels[i]));
        }
    }
    const api::PlotOpts o = quick_opts_of(opts);
    return emit_result(api::render_bar(vals, cats, o), buf, cap, out_len, err);
}

// ── 图像（1:1 包 image::Bitmap，像素不重算）──────────────────────────────────

static image::Bitmap* as_bitmap(tc_image_handle h) { return reinterpret_cast<image::Bitmap*>(h); }

static std::string lower_ascii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return s;
}

// 格式字符串交给媒体注册表解析：注册表是名字的唯一真源，未知名字如实报错。
static bool image_name_from_registry(const char* raw, std::string* out) {
    if (raw == nullptr || raw[0] == '\0') return false;
    const std::string f = lower_ascii(raw);
    if (!media::has_encoder(f)) return false;
    *out = f;
    return true;
}

// 单帧入口的历史契约：静态图 + 静态 AV1；mp4 等动态容器只走多帧入口。
static bool is_single_frame_format(const std::string& name) {
    return name == "png" || name == "jpeg" || name == "jpg" ||
           name == "gif" || name == "webp" || name == "av1" || name == "ivf";
}

static std::string extension_of(const std::string& path) {
    const std::size_t dot = path.find_last_of('.');
    return dot == std::string::npos ? std::string{} : path.substr(dot + 1);
}

static tc_image_handle decode_to_handle(const std::uint8_t* data, std::uint64_t size,
                                        tc_error_handle* err) {
    if (data == nullptr || size == 0) {
        set_err(err, "image decode: empty input");
        return nullptr;
    }
    auto decoded = image::decode_image(data, static_cast<std::size_t>(size));
    if (!decoded.has_value()) {
        set_err(err, "image decode failed (code " +
                         std::to_string(static_cast<int>(decoded.error())) + ")");
        return nullptr;
    }
    return reinterpret_cast<tc_image_handle>(new image::Bitmap(std::move(decoded.value())));
}

static tc_status encode_to_buffer(tc_image_handle image, const char* format, uint8_t quality,
                                  char* buf, std::uint64_t cap, std::uint64_t* out_len,
                                  tc_error_handle* err) {
    image::Bitmap* bmp = as_bitmap(image);
    if (bmp == nullptr) {
        return fail(err, "image encode: null handle", TC_ERR_INVALID_HANDLE);
    }
    std::string name;
    if (!image_name_from_registry(format, &name) || !is_single_frame_format(name)) {
        return fail(err, "image encode: unsupported format '" + std::string(format ? format : "") +
        "' (png / jpeg / gif / webp / av1)",
                    TC_ERR_UNSUPPORTED);
    }
    std::string bytes;
    try {
        image::EncodeOptions opts{};
        opts.quality = quality == 0 ? 85 : static_cast<int>(quality);
        std::array<const image::Bitmap, 1> one{*bmp};
        bytes = media::encode(one, name, opts);
    } catch (const std::exception& ex) {
        return fail(err, std::string("image encode threw: ") + ex.what(), TC_ERR_RENDER);
    } catch (...) {
        return fail(err, "image encode threw: unknown", TC_ERR_RENDER);
    }
    if (out_len != nullptr) {
        *out_len = bytes.size();
    }
    if (buf == nullptr || cap < bytes.size()) {
        return TC_ERR_BUFFER_TOO_SMALL;
    }
    std::memcpy(buf, bytes.data(), bytes.size());
    return TC_OK;
}

tc_image_handle tc_image_create(std::uint32_t width, std::uint32_t height, std::uint8_t channels,
                                tc_error_handle* err) {
    if (width == 0 || height == 0) {
        set_err(err, "image create: width/height must be non-zero");
        return nullptr;
    }
    std::uint8_t c = (channels == 3) ? 3 : 4;
    return reinterpret_cast<tc_image_handle>(new image::Bitmap(width, height, c));
}

tc_image_handle tc_image_decode(const std::uint8_t* data, std::uint64_t size,
                                tc_error_handle* err) {
    return decode_to_handle(data, size, err);
}

tc_image_handle tc_image_load(const char* path, tc_error_handle* err) {
    if (path == nullptr) {
        set_err(err, "image load: null path");
        return nullptr;
    }
    std::FILE* fp = util::fopen_utf8(path, "rb");
    if (fp == nullptr) {
        set_err(err, "image load: cannot open " + std::string(path));
        return nullptr;
    }
    std::string bytes;
    char chunk[8192];
    std::size_t n = 0;
    while ((n = std::fread(chunk, 1, sizeof(chunk), fp)) > 0) {
        bytes.append(chunk, n);
    }
    std::fclose(fp);
    return decode_to_handle(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size(), err);
}

void tc_image_free(tc_image_handle image) { delete as_bitmap(image); }

std::uint32_t tc_image_width(tc_image_handle image) {
    const image::Bitmap* b = as_bitmap(image);
    return b == nullptr ? 0u : b->width();
}

std::uint32_t tc_image_height(tc_image_handle image) {
    const image::Bitmap* b = as_bitmap(image);
    return b == nullptr ? 0u : b->height();
}

std::uint8_t tc_image_channels(tc_image_handle image) {
    const image::Bitmap* b = as_bitmap(image);
    return b == nullptr ? 0u : b->channels();
}

tc_status tc_image_pixels(tc_image_handle image, std::uint8_t* buf, std::uint64_t cap,
                          std::uint64_t* out_len, tc_error_handle* err) {
    const image::Bitmap* b = as_bitmap(image);
    if (b == nullptr) {
        return fail(err, "image pixels: null handle", TC_ERR_INVALID_HANDLE);
    }
    const std::uint64_t need = b->size();
    if (out_len != nullptr) {
        *out_len = need;
    }
    if (buf == nullptr || cap < need) {
        return TC_ERR_BUFFER_TOO_SMALL;
    }
    std::memcpy(buf, b->data(), static_cast<std::size_t>(need));
    return TC_OK;
}

tc_image_handle tc_image_scaled(tc_image_handle image, std::uint32_t width, std::uint32_t height,
                                tc_error_handle* err) {
    const image::Bitmap* b = as_bitmap(image);
    if (b == nullptr) {
        set_err(err, "image scaled: null handle");
        return nullptr;
    }
    if (width == 0 || height == 0) {
        set_err(err, "image scaled: width/height must be non-zero");
        return nullptr;
    }
    return reinterpret_cast<tc_image_handle>(new image::Bitmap(b->scaled_to(width, height)));
}

tc_image_handle tc_image_with_alpha(tc_image_handle image, std::uint8_t alpha,
                                    tc_error_handle* err) {
    const image::Bitmap* b = as_bitmap(image);
    if (b == nullptr) {
        set_err(err, "image with_alpha: null handle");
        return nullptr;
    }
    return reinterpret_cast<tc_image_handle>(new image::Bitmap(b->with_alpha(alpha)));
}

tc_image_handle tc_image_without_alpha(tc_image_handle image, tc_error_handle* err) {
    const image::Bitmap* b = as_bitmap(image);
    if (b == nullptr) {
        set_err(err, "image without_alpha: null handle");
        return nullptr;
    }
    return reinterpret_cast<tc_image_handle>(new image::Bitmap(b->without_alpha()));
}

void tc_image_fill(tc_image_handle image, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                   std::uint8_t a) {
    if (image::Bitmap* dst = as_bitmap(image)) {
        dst->fill(r, g, b, a);
    }
}

void tc_image_fill_rect(tc_image_handle image, std::uint32_t x, std::uint32_t y, std::uint32_t w,
                        std::uint32_t h, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                        std::uint8_t a) {
    if (image::Bitmap* dst = as_bitmap(image)) {
        dst->fill_rect(x, y, w, h, r, g, b, a);
    }
}

void tc_image_blit_over(tc_image_handle dst, tc_image_handle src, std::int32_t dx,
                        std::int32_t dy) {
    image::Bitmap* d = as_bitmap(dst);
    const image::Bitmap* s = as_bitmap(src);
    if (d != nullptr && s != nullptr) {
        d->blit_over(*s, static_cast<int>(dx), static_cast<int>(dy));
    }
}

tc_status tc_image_encode(tc_image_handle image, const char* format, std::uint8_t quality,
                          char* buf, std::uint64_t cap, std::uint64_t* out_len,
                          tc_error_handle* err) {
    return encode_to_buffer(image, format, quality, buf, cap, out_len, err);
}

tc_status tc_image_save(tc_image_handle image, const char* path, const char* format,
                        std::uint8_t quality, tc_error_handle* err) {
    if (path == nullptr || path[0] == '\0') {
        return fail(err, "image save: null path", TC_ERR_INVALID_ARG);
    }
    std::string fmt = format == nullptr || format[0] == '\0' ? extension_of(path) : format;
    std::uint64_t need = 0;
    tc_status st = encode_to_buffer(image, fmt.c_str(), quality, nullptr, 0, &need, err);
    if (st != TC_ERR_BUFFER_TOO_SMALL) {
        return st;  // 其它错误（含格式不支持）直接透出，不静默降级成 PNG
    }
    std::vector<char> buf(static_cast<std::size_t>(need));
    st = encode_to_buffer(image, fmt.c_str(), quality, buf.data(), need, &need, err);
    if (st != TC_OK) {
        return st;
    }
    std::FILE* fp = util::fopen_utf8(path, "wb");
    if (fp == nullptr) {
        return fail(err, "image save: cannot open " + std::string(path), TC_ERR_IO);
    }
    const std::size_t written = std::fwrite(buf.data(), 1, buf.size(), fp);
    std::fclose(fp);
    if (written != buf.size()) {
        return fail(err, "image save: short write to " + std::string(path), TC_ERR_IO);
    }
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

// ── 终端协议 / 关系图 / 文档：辅助实现 ──────────────────────────────────────

namespace {

// 两段式缓冲的唯一写出点（cap=0 只问长度；cap 不够只报错不改数据）。
inline tc_status emit_bytes(const std::string& s, char* buf, std::uint64_t cap,
                            std::uint64_t* out_len, tc_error_handle* err) {
    if (out_len != nullptr) *out_len = static_cast<std::uint64_t>(s.size());
    if (buf == nullptr || cap == 0) return TC_OK;
    if (cap < s.size()) {
        return fail(err, "output buffer too small", TC_ERR_BUFFER_TOO_SMALL);
    }
    if (!s.empty()) std::memcpy(buf, s.data(), s.size());
    return TC_OK;
}

inline protocol::ProtocolKind protocol_kind_of(tc_protocol_kind kind, bool* ok) noexcept {
    switch (kind) {
        case TC_PROTO_AUTO: {
            // AUTO 是唯一会看环境的分支：与 C++ 完全同一条降级链。
            const protocol::ProtocolCaps caps = protocol::detect_protocol_caps();
            *ok = true;
            return protocol::pick_protocol(caps);
        }
        case TC_PROTO_SIXEL:
            *ok = true;
            return protocol::ProtocolKind::Sixel;
        case TC_PROTO_KITTY:
            *ok = true;
            return protocol::ProtocolKind::Kitty;
        case TC_PROTO_ITERM2:
            *ok = true;
            return protocol::ProtocolKind::Iterm2;
        case TC_PROTO_HALFBLOCK:
            *ok = true;
            return protocol::ProtocolKind::HalfBlock;
        default:
            *ok = false;
            return protocol::ProtocolKind::None;
    }
}

// 位图 → 协议字节流。四个编码器都是 protocol/ 里的同一份实现，这里只做选择。
tc_status encode_protocol(const image::Bitmap& bmp, tc_protocol_kind kind,
                          const tc_protocol_opts* po, std::string& out,
                          tc_error_handle* err) {
    if (bmp.empty()) return fail(err, "empty bitmap", TC_ERR_INVALID_ARG);

    protocol::ProtocolOptions o{};
    if (po != nullptr) {
        if (po->cell_width) o.cell_width_px = po->cell_width;
        if (po->cell_height) o.cell_height_px = po->cell_height;
        o.rle = po->rle != 0;
    }

    bool ok = false;
    const protocol::ProtocolKind pk = protocol_kind_of(kind, &ok);
    if (!ok) return fail(err, "unknown protocol kind", TC_ERR_INVALID_ARG);
    if (pk == protocol::ProtocolKind::None) {
        return fail(err, "no terminal image protocol available (sixel/kitty/iterm2/truecolor)",
                    TC_ERR_UNSUPPORTED);
    }

    switch (pk) {
        case protocol::ProtocolKind::Sixel: {
            protocol::SixelProtocol p;
            out = p.encode(bmp, o);
            break;
        }
        case protocol::ProtocolKind::Kitty: {
            protocol::KittyProtocol p;
            out = p.encode(bmp, o);
            break;
        }
        case protocol::ProtocolKind::Iterm2: {
            protocol::Iterm2Inline p;
            out = p.encode(bmp, o);
            break;
        }
        case protocol::ProtocolKind::HalfBlock: {
            protocol::HalfBlockProtocol p;
            out = p.encode(bmp, o);
            break;
        }
        case protocol::ProtocolKind::None:
            break;
    }
    if (out.empty()) {
        return fail(err, std::string("protocol encode failed (") + protocol::protocol_name(pk) + ")",
                    TC_ERR_RENDER);
    }
    return TC_OK;
}

// ── 关系图 ──────────────────────────────────────────────────────────────────

// 12 个图种共用一张句柄：kind 决定布局 + 渲染器，其余全是可选覆盖。
struct GraphData {
    graph::Graph graph;
    std::string kind;

    std::optional<graph::ShapeKind> shape;
    std::optional<graph::RouteStyle> route;
    std::optional<bool> edge_labels;
    std::optional<bool> arrows;
    std::optional<bool> node_border;
    std::optional<bool> mark_cycles;
    std::optional<double> min_width;
    std::optional<double> min_height;
    std::optional<double> h_gap;
    std::optional<double> v_gap;

    Style node_style{};
    Style edge_style{};
};

// 能力表：哪条属性对哪个图种有意义。表里为 false 的组合一律 TC_ERR_UNSUPPORTED，
// 不静默丢弃——图种选项结构体里根本没有那个字段，硬塞只能靠"猜一个相近的"。
struct GraphKindDef {
    const char* name;
    bool shape;
    bool route;
    bool edge_labels;
    bool arrows;
    bool node_border;
    bool layout;   // min_width / min_height / h_gap / v_gap
    bool cycles;   // dependency 的环标记
};

constexpr GraphKindDef kGraphKinds[] = {
    // name              shape  route  labels arrows border layout cycles
    {"flowchart",        true,  true,  false, false, false, true,  false},
    {"state_machine",    true,  true,  false, false, false, true,  false},
    {"mindmap",          true,  false, false, false, true,  true,  false},
    {"orgchart",         true,  false, false, false, true,  true,  false},
    {"dag",              true,  false, true,  true,  false, true,  false},
    {"dependency",       true,  false, true,  true,  false, true,  true},
    {"er_diagram",       false, true,  false, false, false, true,  false},
    {"knowledge_graph",  false, true,  true,  true,  true,  true,  false},
    {"social_network",   false, false, false, true,  true,  true,  false},
    {"uml_activity",     false, true,  false, false, false, true,  false},
    {"uml_class",        false, true,  false, false, false, true,  false},
    {"uml_usecase",      false, false, false, false, false, true,  false},
};

inline const GraphKindDef* find_graph_kind(std::string_view name) noexcept {
    for (const GraphKindDef& d : kGraphKinds) {
        if (name == d.name) return &d;
    }
    return nullptr;
}

inline GraphData* as_graph(tc_graph_handle h) noexcept {
    return reinterpret_cast<GraphData*>(h);
}

inline void tune_layout(const GraphData& d, graph::LayoutOptions& lo) {
    if (d.min_width) lo.min_width = *d.min_width;
    if (d.min_height) lo.min_height = *d.min_height;
    if (d.h_gap) lo.h_gap = *d.h_gap;
    if (d.v_gap) lo.v_gap = *d.v_gap;
}

// 树布局的"兄弟间距"叫 row_gap，语义就是垂直间距。
inline void tune_tree(const GraphData& d, graph::TreeOptions& to) {
    tune_layout(d, to.layout);
    if (d.v_gap) to.row_gap = *d.v_gap;
}

// 就地布局 + 绘制。每个分支只做"选项结构体搬运"，没有任何绘制逻辑。
Rect draw_graph(GraphData& d, canvas::Canvas& cv) {
    const std::string& k = d.kind;

    if (k == "flowchart") {
        graph::FlowchartOptions o;
        tune_layout(d, o.layout);
        if (d.shape) o.shape = *d.shape;
        if (d.route) o.route_style = *d.route;
        o.node_style = d.node_style;
        o.edge_style = d.edge_style;
        return graph::render_flowchart(d.graph, cv, o);
    }
    if (k == "state_machine") {
        graph::StateMachineOptions o;
        tune_layout(d, o.layout);
        if (d.shape) o.shape = *d.shape;
        if (d.route) o.route_style = *d.route;
        o.node_style = d.node_style;
        o.edge_style = d.edge_style;
        return graph::render_state_machine(d.graph, cv, o);
    }
    if (k == "mindmap") {
        graph::MindmapOptions o;
        tune_tree(d, o.tree);
        if (d.shape) o.shape = *d.shape;
        if (d.node_border) o.node_border = *d.node_border;
        o.topic_style = d.node_style;
        o.branch_style = d.edge_style;
        return graph::render_mindmap(d.graph, cv, o);
    }
    if (k == "orgchart") {
        graph::OrgChartOptions o;
        tune_tree(d, o.tree);
        if (d.shape) o.shape = *d.shape;
        if (d.node_border) o.node_border = *d.node_border;
        o.node_style = d.node_style;
        o.edge_style = d.edge_style;
        return graph::render_orgchart(d.graph, cv, o);
    }
    if (k == "dag" || k == "dependency") {
        graph::DagOptions o;
        tune_layout(d, o.layout);
        if (d.shape) o.shape = *d.shape;
        if (d.arrows) o.arrows = *d.arrows;
        if (d.edge_labels) o.edge_labels = *d.edge_labels;
        o.node_style = d.node_style;
        o.edge_style = d.edge_style;
        if (k == "dag") return graph::render_dag(d.graph, cv, o);

        graph::DependencyOptions dep;
        dep.dag = o;
        if (d.mark_cycles) dep.mark_cycles = *d.mark_cycles;
        return graph::render_dependency(d.graph, cv, dep);
    }
    if (k == "er_diagram") {
        graph::ErDiagramOptions o;
        tune_layout(d, o.layout);
        if (d.route) o.route_style = *d.route;
        o.entity_style = d.node_style;
        o.name_style = d.node_style;
        o.field_style = d.node_style;
        o.edge_style = d.edge_style;
        return graph::render_er_diagram(d.graph, cv, o);
    }
    if (k == "knowledge_graph") {
        graph::KnowledgeGraphOptions o;
        tune_layout(d, o.force.layout);
        if (d.route) o.route_style = *d.route;
        if (d.node_border) o.node_border = *d.node_border;
        if (d.arrows) o.arrows = *d.arrows;
        if (d.edge_labels) o.edge_labels = *d.edge_labels;
        o.node_style = d.node_style;
        o.edge_style = d.edge_style;
        return graph::render_knowledge_graph(d.graph, cv, o);
    }
    if (k == "social_network") {
        graph::SocialNetworkOptions o;
        tune_layout(d, o.layout);
        if (d.node_border) o.node_border = *d.node_border;
        if (d.arrows) o.arrows = *d.arrows;
        o.node_style = d.node_style;
        o.edge_style = d.edge_style;
        return graph::render_social_network(d.graph, cv, o);
    }
    if (k == "uml_activity") {
        graph::UmlActivityOptions o;
        tune_layout(d, o.layout);
        if (d.route) o.route_style = *d.route;
        o.node_style = d.node_style;
        o.decision_style = d.node_style;
        o.edge_style = d.edge_style;
        return graph::render_uml_activity(d.graph, cv, o);
    }
    if (k == "uml_class") {
        graph::UmlClassOptions o;
        tune_layout(d, o.layout);
        if (d.route) o.route_style = *d.route;
        o.border_style = d.node_style;
        o.name_style = d.node_style;
        o.member_style = d.node_style;
        o.edge_style = d.edge_style;
        return graph::render_uml_class(d.graph, cv, o);
    }
    // uml_usecase（create 已校验 kind，这里是最后一条登记项）
    graph::UmlUsecaseOptions o;
    tune_layout(d, o.layout);
    if (d.h_gap) o.h_gap = *d.h_gap;  // 参与者列与用例列之间那一段
    o.actor_style = d.node_style;
    o.usecase_style = d.node_style;
    o.edge_style = d.edge_style;
    return graph::render_uml_usecase(d.graph, cv, o);
}

inline std::uint32_t clamp_dim(double v) noexcept {
    const double c = std::ceil(v);
    if (!(c >= 1.0)) return 1;  // 0 / 负 / NaN
    if (c > 200.0) return 200;
    return static_cast<std::uint32_t>(c);
}

// 自适应尺寸走"先探测包围盒、再按目标尺寸重画一遍"：布局与画布尺寸无关，
// 因此两遍得到的位置完全一致，第二遍只是把输出落在正确的画布上。
tc_status render_graph_cells(GraphData& d, const tc_plot_opts* o, CellBuffer& out,
                             tc_error_handle* err) {
    if (d.graph.node_count() == 0) return fail(err, "empty graph", TC_ERR_INVALID_ARG);

    std::uint32_t cols = (o && o->width) ? o->width : 0;
    std::uint32_t rows = (o && o->height) ? o->height : 0;
    if (cols > 0xFFFF || rows > 0xFFFF) {
        return fail(err, "width/height out of range", TC_ERR_INVALID_ARG);
    }
    if (cols == 0 || rows == 0) {
        canvas::Canvas probe(200, 100);  // 上限即头文件承诺的 200×100
        const Rect r = draw_graph(d, probe);
        if (cols == 0) cols = clamp_dim(r.x + r.w + 1.0);
        if (rows == 0) rows = clamp_dim(r.y + r.h + 1.0);
    }

    canvas::Canvas cv(cols, rows);
    draw_graph(d, cv);
    out = cv.to_buffer();
    return TC_OK;
}

inline tc_status render_graph_bytes(GraphData& d, const tc_plot_opts* opts, std::string& out,
                                    tc_error_handle* err) {
    CellBuffer cells;
    const tc_status st = render_graph_cells(d, opts, cells, err);
    if (st != TC_OK) return st;
    const ColorDepth depth = (opts && opts->depth != TC_DEPTH_NONE) ? to_depth(opts->depth)
                                                                    : ColorDepth::TrueColor;
    render::Renderer renderer(depth);
    out = renderer.render_full(cells);
    return TC_OK;
}

// 图的"像素化"与图表同一条路：CellBuffer → 位图，供 PNG / 协议共用。
image::Bitmap graph_bitmap(GraphData& d, const tc_plot_opts* opts, tc_error_handle* err,
                           tc_status* status) {
    CellBuffer cells;
    *status = render_graph_cells(d, opts, cells, err);
    if (*status != TC_OK) return image::Bitmap{};
    gui::CellBitmapOptions copts{};
    return gui::render_cells_to_bitmap(cells, copts);
}

inline std::string graph_kind_or_default(const char* kind) {
    return (kind && kind[0] != '\0') ? std::string(kind) : std::string("flowchart");
}

}  // namespace

extern "C" {

// ── 终端协议 ────────────────────────────────────────────────────────────────

tc_status tc_image_to_protocol(tc_image_handle image, tc_protocol_kind kind,
                               const tc_protocol_opts* opts, char* buf, std::uint64_t cap,
                               std::uint64_t* out_len, tc_error_handle* err) {
    if (image == nullptr) return fail(err, "null image", TC_ERR_INVALID_HANDLE);
    if (out_len == nullptr) return fail(err, "null out_len", TC_ERR_INVALID_ARG);
    std::string bytes;
    const tc_status st = encode_protocol(*as_bitmap(image), kind, opts, bytes, err);
    if (st != TC_OK) return st;
    return emit_bytes(bytes, buf, cap, out_len, err);
}

tc_status tc_protocol_detect(char* buf, std::uint64_t cap, std::uint64_t* out_len,
                             tc_error_handle* err) {
    if (out_len == nullptr) return fail(err, "null out_len", TC_ERR_INVALID_ARG);
    const std::string name(protocol::protocol_name(protocol::pick_protocol(
        protocol::detect_protocol_caps())));
    return emit_bytes(name, buf, cap, out_len, err);
}

tc_status tc_chart_to_protocol(tc_chart_handle chart, tc_protocol_kind kind,
                               const tc_plot_opts* opts, const tc_protocol_opts* proto_opts,
                               char* buf, std::uint64_t cap, std::uint64_t* out_len,
                               tc_error_handle* err) {
    if (chart == nullptr) return fail(err, "null chart", TC_ERR_INVALID_HANDLE);
    if (out_len == nullptr) return fail(err, "null out_len", TC_ERR_INVALID_ARG);
    auto* c = reinterpret_cast<ChartData*>(chart);
    if (!has_data(*c)) return fail(err, "empty chart data", TC_ERR_INVALID_ARG);

    CellBuffer cells;
    if (!build_plot_with_labels(*c, opts, cells)) {
        return fail(err, "invalid width/height", TC_ERR_INVALID_ARG);
    }
    gui::CellBitmapOptions copts{};
    const image::Bitmap bmp = gui::render_cells_to_bitmap(cells, copts);
    std::string bytes;
    const tc_status st = encode_protocol(bmp, kind, proto_opts, bytes, err);
    if (st != TC_OK) return st;
    return emit_bytes(bytes, buf, cap, out_len, err);
}

// ── 关系图 ──────────────────────────────────────────────────────────────────

tc_graph_handle tc_graph_create(const char* kind, tc_error_handle* err) {
    const std::string name = graph_kind_or_default(kind);
    if (find_graph_kind(name) == nullptr) {
        set_err(err, "unknown graph kind: " + name);
        return nullptr;
    }
    GraphData* d = new (std::nothrow) GraphData();
    if (d == nullptr) {
        set_err(err, "out of memory");
        return nullptr;
    }
    d->kind = name;
    return reinterpret_cast<tc_graph_handle>(d);
}

tc_graph_handle tc_graph_from_text(const char* text, const char* format, const char* kind,
                                   tc_error_handle* err) {
    if (text == nullptr) {
        set_err(err, "null text");
        return nullptr;
    }
    const std::string name = graph_kind_or_default(kind);
    if (find_graph_kind(name) == nullptr) {
        set_err(err, "unknown graph kind: " + name);
        return nullptr;
    }

    dsl::register_builtin_dsls();  // 幂等；首次解析前也可能没登记
    const dsl::DslRegistry& reg = dsl::DslRegistry::instance();
    const std::string_view src(text);

    std::optional<dsl::AstGraph> ast;
    if (format != nullptr && format[0] != '\0') {
        auto parsed = reg.parse(format, src);
        if (!parsed.has_value()) {
            set_err(err, std::string("parse failed (") + format + ", " + describe(parsed.error()) + ")");
            return nullptr;
        }
        ast = std::move(parsed.value());
    } else {
        // 没指定方言：依次尝试注册表里的名字，第一个解析成功者胜出。
        for (std::string_view n : reg.names()) {
            auto parsed = reg.parse(n, src);
            if (parsed.has_value()) {
                ast = std::move(parsed.value());
                break;
            }
        }
        if (!ast) {
            set_err(err, "parse failed (no registered dialect accepted the text)");
            return nullptr;
        }
    }

    auto g = dsl::to_graph(*ast);
    if (!g.has_value()) {
        set_err(err, std::string("ast_to_graph failed (") + describe(g.error()) + ")");
        return nullptr;
    }

    GraphData* d = new (std::nothrow) GraphData();
    if (d == nullptr) {
        set_err(err, "out of memory");
        return nullptr;
    }
    d->kind = name;
    d->graph = std::move(g.value());
    return reinterpret_cast<tc_graph_handle>(d);
}

void tc_graph_free(tc_graph_handle graph) { delete as_graph(graph); }

uint64_t tc_graph_add_node(tc_graph_handle graph, const char* label) {
    GraphData* d = as_graph(graph);
    if (d == nullptr) return 0;
    graph::Node& n = d->graph.add_node(label == nullptr ? std::string{} : std::string(label));
    return static_cast<uint64_t>(n.id());
}

tc_status tc_graph_add_edge(tc_graph_handle graph, uint64_t from, uint64_t to, const char* label,
                            tc_error_handle* err) {
    GraphData* d = as_graph(graph);
    if (d == nullptr) return fail(err, "null graph", TC_ERR_INVALID_HANDLE);
    graph::Edge* e = d->graph.add_edge(static_cast<graph::NodeId>(from),
                                       static_cast<graph::NodeId>(to),
                                       label == nullptr ? std::string{} : std::string(label));
    if (e == nullptr) return fail(err, "edge endpoint does not exist", TC_ERR_INVALID_ARG);
    return TC_OK;
}

tc_status tc_graph_set_node_attr(tc_graph_handle graph, uint64_t node, const char* key,
                                 const char* value, tc_error_handle* err) {
    GraphData* d = as_graph(graph);
    if (d == nullptr) return fail(err, "null graph", TC_ERR_INVALID_HANDLE);
    if (key == nullptr || key[0] == '\0') return fail(err, "null attribute key", TC_ERR_INVALID_ARG);
    graph::Node* n = d->graph.find(static_cast<graph::NodeId>(node));
    if (n == nullptr) return fail(err, "node does not exist", TC_ERR_INVALID_ARG);
    n->attributes().set(key, value == nullptr ? std::string{} : std::string(value));
    return TC_OK;
}

tc_status tc_graph_set_edge_attr(tc_graph_handle graph, uint64_t from, uint64_t to,
                                 const char* key, const char* value, tc_error_handle* err) {
    GraphData* d = as_graph(graph);
    if (d == nullptr) return fail(err, "null graph", TC_ERR_INVALID_HANDLE);
    if (key == nullptr || key[0] == '\0') return fail(err, "null attribute key", TC_ERR_INVALID_ARG);
    graph::Edge* e = d->graph.find_edge(static_cast<graph::NodeId>(from),
                                        static_cast<graph::NodeId>(to));
    if (e == nullptr) return fail(err, "edge does not exist", TC_ERR_INVALID_ARG);
    e->attributes().set(key, value == nullptr ? std::string{} : std::string(value));
    return TC_OK;
}

tc_status tc_graph_set_prop(tc_graph_handle graph, tc_graph_prop prop, const tc_value* value,
                            tc_error_handle* err) {
    GraphData* d = as_graph(graph);
    if (d == nullptr) return fail(err, "null graph", TC_ERR_INVALID_HANDLE);
    if (value == nullptr) return fail(err, "null property value", TC_ERR_INVALID_ARG);
    const GraphKindDef* def = find_graph_kind(d->kind);
    if (def == nullptr) return fail(err, "unknown graph kind: " + d->kind, TC_ERR_INVALID_HANDLE);

    bool b = false;
    double v = 0.0;
    switch (prop) {
        case TC_GRAPH_PROP_SHAPE: {
            if (!def->shape) return fail(err, d->kind + " does not support shape", TC_ERR_UNSUPPORTED);
            if (value->kind != TC_VAL_INT || value->i < 0 || value->i > 7) {
                return fail(err, "shape must be TC_VAL_INT in [0,7]", TC_ERR_INVALID_ARG);
            }
            d->shape = static_cast<graph::ShapeKind>(value->i);
            return TC_OK;
        }
        case TC_GRAPH_PROP_ROUTE: {
            if (!def->route) return fail(err, d->kind + " does not support route", TC_ERR_UNSUPPORTED);
            if (value->kind != TC_VAL_INT || value->i < 0 || value->i > 1) {
                return fail(err, "route must be TC_VAL_INT in [0,1]", TC_ERR_INVALID_ARG);
            }
            d->route = static_cast<graph::RouteStyle>(value->i);
            return TC_OK;
        }
        case TC_GRAPH_PROP_EDGE_LABELS:
            if (!def->edge_labels) {
                return fail(err, d->kind + " does not support edge labels", TC_ERR_UNSUPPORTED);
            }
            if (!bool_of(*value, b)) return fail(err, "edge_labels must be bool", TC_ERR_INVALID_ARG);
            d->edge_labels = b;
            return TC_OK;
        case TC_GRAPH_PROP_ARROWS:
            if (!def->arrows) return fail(err, d->kind + " does not support arrows", TC_ERR_UNSUPPORTED);
            if (!bool_of(*value, b)) return fail(err, "arrows must be bool", TC_ERR_INVALID_ARG);
            d->arrows = b;
            return TC_OK;
        case TC_GRAPH_PROP_NODE_BORDER:
            if (!def->node_border) {
                return fail(err, d->kind + " does not support node border", TC_ERR_UNSUPPORTED);
            }
            if (!bool_of(*value, b)) return fail(err, "node_border must be bool", TC_ERR_INVALID_ARG);
            d->node_border = b;
            return TC_OK;
        case TC_GRAPH_PROP_MARK_CYCLES:
            if (!def->cycles) return fail(err, d->kind + " does not support mark_cycles", TC_ERR_UNSUPPORTED);
            if (!bool_of(*value, b)) return fail(err, "mark_cycles must be bool", TC_ERR_INVALID_ARG);
            d->mark_cycles = b;
            return TC_OK;
        case TC_GRAPH_PROP_MIN_WIDTH:
        case TC_GRAPH_PROP_MIN_HEIGHT:
        case TC_GRAPH_PROP_H_GAP:
        case TC_GRAPH_PROP_V_GAP: {
            if (!def->layout) return fail(err, d->kind + " does not support layout tuning", TC_ERR_UNSUPPORTED);
            if (value->kind == TC_VAL_DOUBLE) {
                v = value->d;
            } else if (value->kind == TC_VAL_INT) {
                v = static_cast<double>(value->i);
            } else if (is_clear(*value)) {
                v = 0.0;  // 清除 = 回到图种默认
            } else {
                return fail(err, "layout property must be double", TC_ERR_INVALID_ARG);
            }
            std::optional<double>* slot = nullptr;
            switch (prop) {
                case TC_GRAPH_PROP_MIN_WIDTH: slot = &d->min_width; break;
                case TC_GRAPH_PROP_MIN_HEIGHT: slot = &d->min_height; break;
                case TC_GRAPH_PROP_H_GAP: slot = &d->h_gap; break;
                default: slot = &d->v_gap; break;
            }
            if (!(v > 0.0)) slot->reset();
            else *slot = v;
            return TC_OK;
        }
        default:
            return fail(err, "unknown graph property", TC_ERR_INVALID_ARG);
    }
}

tc_status tc_graph_set_style(tc_graph_handle graph, const tc_style* node_style,
                             const tc_style* edge_style, tc_error_handle* err) {
    GraphData* d = as_graph(graph);
    if (d == nullptr) return fail(err, "null graph", TC_ERR_INVALID_HANDLE);
    if (node_style != nullptr) d->node_style = to_style(node_style);
    if (edge_style != nullptr) d->edge_style = to_style(edge_style);
    return TC_OK;
}

uint64_t tc_graph_node_count(tc_graph_handle graph) {
    GraphData* d = as_graph(graph);
    return d == nullptr ? 0 : static_cast<uint64_t>(d->graph.node_count());
}

uint64_t tc_graph_edge_count(tc_graph_handle graph) {
    GraphData* d = as_graph(graph);
    return d == nullptr ? 0 : static_cast<uint64_t>(d->graph.edge_count());
}

tc_status tc_graph_render_ansi(tc_graph_handle graph, const tc_plot_opts* opts, char* buf,
                               std::uint64_t cap, std::uint64_t* out_len, tc_error_handle* err) {
    GraphData* d = as_graph(graph);
    if (d == nullptr) return fail(err, "null graph", TC_ERR_INVALID_HANDLE);
    if (out_len == nullptr) return fail(err, "null out_len", TC_ERR_INVALID_ARG);
    std::string bytes;
    const tc_status st = render_graph_bytes(*d, opts, bytes, err);
    if (st != TC_OK) return st;
    return emit_bytes(bytes, buf, cap, out_len, err);
}

tc_status tc_graph_print(tc_graph_handle graph, const tc_plot_opts* opts, tc_error_handle* err) {
    GraphData* d = as_graph(graph);
    if (d == nullptr) return fail(err, "null graph", TC_ERR_INVALID_HANDLE);
    std::string bytes;
    const tc_status st = render_graph_bytes(*d, opts, bytes, err);
    if (st != TC_OK) return st;
    const Status s = api::write_stdout(bytes);
    if (!is_ok(s)) {
        return fail(err, std::string("write_stdout failed (") + describe(s) + ")", TC_ERR_IO);
    }
    return TC_OK;
}

tc_status tc_graph_to_png(tc_graph_handle graph, const tc_plot_opts* opts, char* buf,
                          std::uint64_t cap, std::uint64_t* out_len, tc_error_handle* err) {
    GraphData* d = as_graph(graph);
    if (d == nullptr) return fail(err, "null graph", TC_ERR_INVALID_HANDLE);
    if (out_len == nullptr) return fail(err, "null out_len", TC_ERR_INVALID_ARG);
    tc_status st = TC_OK;
    const image::Bitmap bmp = graph_bitmap(*d, opts, err, &st);
    if (st != TC_OK) return st;
    const std::string png = image::encode(bmp, image::ImageFormat::Png);
    if (png.empty()) return fail(err, "png encode failed", TC_ERR_RENDER);
    return emit_bytes(png, buf, cap, out_len, err);
}

tc_status tc_graph_to_protocol(tc_graph_handle graph, tc_protocol_kind kind,
                               const tc_plot_opts* opts, const tc_protocol_opts* proto_opts,
                               char* buf, std::uint64_t cap, std::uint64_t* out_len,
                               tc_error_handle* err) {
    GraphData* d = as_graph(graph);
    if (d == nullptr) return fail(err, "null graph", TC_ERR_INVALID_HANDLE);
    if (out_len == nullptr) return fail(err, "null out_len", TC_ERR_INVALID_ARG);
    tc_status st = TC_OK;
    const image::Bitmap bmp = graph_bitmap(*d, opts, err, &st);
    if (st != TC_OK) return st;
    std::string bytes;
    st = encode_protocol(bmp, kind, proto_opts, bytes, err);
    if (st != TC_OK) return st;
    return emit_bytes(bytes, buf, cap, out_len, err);
}

}  // extern "C"

// ── 文档：块序列 → 一次性导出 ───────────────────────────────────────────────

namespace {

struct DocData {
    document::Document doc;
};

inline DocData* as_doc(tc_doc_handle h) noexcept {
    return reinterpret_cast<DocData*>(h);
}

// 段落是 Span 序列；C 面只给纯文本，所以每条就是一个无样式 Span。
inline std::vector<document::Span> spans_of(const char* text) {
    std::vector<document::Span> spans;
    spans.emplace_back(text == nullptr ? std::string{} : std::string(text));
    return spans;
}

// 三种 format 全部走同一份实现：plain = Document 自带序列化，html / markdown = structured 后端。
tc_status document_text(const document::Document& doc, const char* format, const char* title,
                        const char* css_class, std::string& out, tc_error_handle* err) {
    const std::string fmt = format == nullptr ? std::string{} : lower_ascii(format);
    backend::structured::StructuredOptions so;
    if (title != nullptr) so.title = title;
    if (css_class != nullptr) so.css_class = css_class;

    if (fmt == "plain" || fmt == "text") {
        out = doc.to_plain_text();
    } else if (fmt == "html") {
        backend::structured::HtmlBackend b;
        b.set_options(so);
        out = backend::structured::render_structured(b, doc);
    } else if (fmt == "markdown" || fmt == "md") {
        backend::structured::MarkdownBackend b;
        b.set_options(so);
        out = backend::structured::render_structured(b, doc);
    } else {
        return fail(err, "unknown document format (expected html/markdown/plain)",
                    TC_ERR_INVALID_ARG);
    }
    if (out.empty()) return fail(err, "document export produced no output", TC_ERR_RENDER);
    return TC_OK;
}

}  // namespace

extern "C" {

tc_doc_handle tc_doc_create(tc_error_handle* err) {
    DocData* d = new (std::nothrow) DocData();
    if (d == nullptr) {
        set_err(err, "out of memory");
        return nullptr;
    }
    return reinterpret_cast<tc_doc_handle>(d);
}

void tc_doc_free(tc_doc_handle doc) { delete as_doc(doc); }

tc_status tc_doc_add_heading(tc_doc_handle doc, const char* text, std::uint32_t level,
                             tc_error_handle* err) {
    DocData* d = as_doc(doc);
    if (d == nullptr) return fail(err, "null document", TC_ERR_INVALID_HANDLE);
    // Block::heading 的 level 语义是 1..6，越界由这里收口而不是让后端猜。
    std::uint32_t lv = level;
    if (lv < 1) lv = 1;
    if (lv > 6) lv = 6;
    d->doc.add_block(document::Block::heading(
        text == nullptr ? std::string{} : std::string(text), static_cast<int>(lv)));
    return TC_OK;
}

tc_status tc_doc_add_paragraph(tc_doc_handle doc, const char* text, tc_error_handle* err) {
    DocData* d = as_doc(doc);
    if (d == nullptr) return fail(err, "null document", TC_ERR_INVALID_HANDLE);
    d->doc.add_block(document::Block::paragraph(spans_of(text)));
    return TC_OK;
}

tc_status tc_doc_add_code(tc_doc_handle doc, const char* text, const char* language,
                          tc_error_handle* err) {
    DocData* d = as_doc(doc);
    if (d == nullptr) return fail(err, "null document", TC_ERR_INVALID_HANDLE);
    d->doc.add_block(document::Block::code(text == nullptr ? std::string{} : std::string(text),
                                          language == nullptr ? std::string{} : std::string(language)));
    return TC_OK;
}

tc_status tc_doc_add_table(tc_doc_handle doc, const char* const* header, std::uint64_t n_cols,
                           const char* const* const* rows, std::uint64_t row_count,
                           tc_error_handle* err) {
    DocData* d = as_doc(doc);
    if (d == nullptr) return fail(err, "null document", TC_ERR_INVALID_HANDLE);
    if (n_cols == 0) return fail(err, "table needs at least one column", TC_ERR_INVALID_ARG);

    document::TableModel model;
    if (header != nullptr) {
        std::vector<std::string> cols;
        cols.reserve(static_cast<std::size_t>(n_cols));
        for (std::uint64_t i = 0; i < n_cols; ++i) {
            const char* cell = header[i];
            cols.emplace_back(cell == nullptr ? std::string{} : std::string(cell));
        }
        model.set_header(std::move(cols));
    }
    if (rows != nullptr) {
        for (std::uint64_t r = 0; r < row_count; ++r) {
            const char* const* row = rows[r];
            std::vector<std::string> cells;
            cells.reserve(static_cast<std::size_t>(n_cols));
            for (std::uint64_t c = 0; c < n_cols; ++c) {
                const char* cell = (row == nullptr) ? nullptr : row[c];
                cells.emplace_back(cell == nullptr ? std::string{} : std::string(cell));
            }
            model.add_row(std::move(cells));
        }
    }
    d->doc.add_block(document::Block::table(std::move(model)));
    return TC_OK;
}

tc_status tc_doc_add_chart(tc_doc_handle doc, tc_chart_handle chart, std::uint32_t width,
                          std::uint32_t height, tc_error_handle* err) {
    DocData* d = as_doc(doc);
    if (d == nullptr) return fail(err, "null document", TC_ERR_INVALID_HANDLE);
    if (chart == nullptr) return fail(err, "null chart", TC_ERR_INVALID_HANDLE);
    auto* c = reinterpret_cast<ChartData*>(chart);
    if (!has_data(*c)) return fail(err, "empty chart data", TC_ERR_INVALID_ARG);

    const std::uint32_t w = width != 0 ? width : 80;
    const std::uint32_t h = height != 0 ? height : 24;

    // 与 tc_chart_to_svg 完全同一条通路：同一份几何、同一份轴标签补绘。
    chart::Chart& ch = apply_display(*c, nullptr);
    auto sc = api::chart_to_scene(ch, w, h);
    if (!sc.has_value()) {
        return fail(err, std::string("chart_to_scene failed (") + describe(sc.error()) + ")",
                    TC_ERR_UNSUPPORTED);
    }
    scene::Scene scene = std::move(sc.value());
    decorate_scene_labels(scene, *c);
    d->doc.add_block(document::Block::chart(std::move(scene)));
    return TC_OK;
}

tc_status tc_doc_export(tc_doc_handle doc, const char* format, const char* title,
                        const char* css_class, char* buf, std::uint64_t cap, std::uint64_t* out_len,
                        tc_error_handle* err) {
    DocData* d = as_doc(doc);
    if (d == nullptr) return fail(err, "null document", TC_ERR_INVALID_HANDLE);
    if (out_len == nullptr) return fail(err, "null out_len", TC_ERR_INVALID_ARG);
    std::string text;
    const tc_status st = document_text(d->doc, format, title, css_class, text, err);
    if (st != TC_OK) return st;
    return emit_bytes(text, buf, cap, out_len, err);
}

tc_status tc_doc_save(tc_doc_handle doc, const char* path, const char* format, const char* title,
                      const char* css_class, tc_error_handle* err) {
    DocData* d = as_doc(doc);
    if (d == nullptr) return fail(err, "null document", TC_ERR_INVALID_HANDLE);
    if (path == nullptr || path[0] == '\0') return fail(err, "null path", TC_ERR_INVALID_ARG);
    std::string text;
    const tc_status st = document_text(d->doc, format, title, css_class, text, err);
    if (st != TC_OK) return st;
    std::FILE* fp = util::fopen_utf8(path, "wb");
    if (fp == nullptr) return fail(err, "cannot open " + std::string(path), TC_ERR_IO);
    const std::size_t written = std::fwrite(text.data(), 1, text.size(), fp);
    std::fclose(fp);
    if (written != text.size()) {
        return fail(err, "short write to " + std::string(path), TC_ERR_IO);
    }
    return TC_OK;
}

}  // extern "C"

// ────────────────────────────────────────────────────────────────────────────
// 终端控制 / 自绘 / 增量渲染 / 输入与命中 / 导出物种 / 时序图 / 多帧 / 3D·4D
// ────────────────────────────────────────────────────────────────────────────

namespace {

struct TermHandle {
    explicit TermHandle(term::TerminalOptions o) : t(o) {}
    term::Terminal t;
};

struct CanvasHandle {
    CanvasHandle(std::uint32_t w, std::uint32_t h) : cv(w, h) {}
    canvas::Canvas cv;
};

struct RendererHandle {
    explicit RendererHandle(ColorDepth d) : r(d) {}
    render::Renderer r;
    // 两段式缓冲：cap=0 的查询也会推进 Renderer 的 prev，必须缓存该帧，
    // 否则随后按长度取值时拿到的是"与刚更新的 prev 比较"的空结果。
    std::string cached;
    bool has_cache = false;
};

struct InputHandle {
    interaction::InputParser parser;
    std::vector<std::string> pastes;  // tc_event.text 指向这里，下次 feed 前有效
    // 同理：feed 会消耗字节流，cap=0 的查询必须缓存事件供随后取用。
    std::vector<interaction::Event> cached_events;
    std::vector<std::string> cached_pastes;
    bool has_cache = false;
};

struct RegionsHandle {
    std::vector<chart::HitRegion> regions;
};

struct SeqHandle {
    graph::SeqModel model;
};

struct Scene3dHandle {
    std::string kind;
    std::vector<double> points;   // scatter3d: xyz / wire4d: xyzw
    std::vector<double> grid;     // surface3d: z（行主序）
    std::uint32_t grid_cols = 0;
    std::uint32_t grid_rows = 0;
    std::vector<std::uint32_t> edges;
    double x0 = 0.0, x1 = 1.0, y0 = 0.0, y1 = 1.0;
    double azimuth = 35.0, elevation = 25.0, fovy = 55.0;
    double rot_xw = 20.0, rot_yz = 15.0;
};

inline Style style_or_default(const tc_style* s) noexcept {
    return s ? to_style(s) : Style{};
}

inline ColorDepth depth_of(std::uint8_t d) noexcept {
    return d ? to_depth(static_cast<tc_color_depth>(d)) : ColorDepth::TrueColor;
}

// CellBuffer → 统一输出。需要 Scene 的格式（SVG/PDF/EPS）在此显式拒绝：
// 持有 Scene 的对象（chart_export / scene3d_render）自己走矢量路径。
inline tc_status render_cells_to_output(const CellBuffer& cells, const tc_output_opts* o,
                                        std::string& out, tc_error_handle* err) {
    const int fmt = o ? static_cast<int>(o->format) : TC_OUT_ANSI;
    const ColorDepth depth = depth_of(o ? o->color_depth : 0);
    switch (fmt) {
        case TC_OUT_ANSI: {
            render::Renderer r(depth);
            out = r.render_full(cells);
            break;
        }
        case TC_OUT_TEXT: {
            backend::snapshot::TextBackend b;
            backend::snapshot::SnapshotOptions so;
            so.depth = depth;
            b.set_options(so);
            out = backend::snapshot::render_snapshot(b, cells);
            break;
        }
        case TC_OUT_HTML_PRE: {
            backend::snapshot::HtmlPreBackend b;
            backend::snapshot::SnapshotOptions so;
            so.depth = depth;
            b.set_options(so);
            out = backend::snapshot::render_snapshot(b, cells);
            break;
        }
        case TC_OUT_JSON: {
            backend::snapshot::JsonBackend b;
            out = backend::snapshot::render_snapshot(b, cells);
            break;
        }
        case TC_OUT_LATEX: {
            backend::vector::LatexBackend b;
            out = backend::vector::render_latex(b, cells);
            break;
        }
        case TC_OUT_PNG:
        case TC_OUT_PROTOCOL: {
            gui::CellBitmapOptions co{};
            const image::Bitmap bmp = gui::render_cells_to_bitmap(cells, co);
            if (fmt == TC_OUT_PNG) {
                out = image::encode(bmp, image::ImageFormat::Png);
            } else {
                tc_protocol_opts po{};
                po.cell_width = (o && o->cell_width) ? o->cell_width : 10u;
                po.cell_height = (o && o->cell_height) ? o->cell_height : 20u;
                const auto kind = static_cast<tc_protocol_kind>(o ? o->protocol
                                                                   : static_cast<std::uint8_t>(TC_PROTO_AUTO));
                return encode_protocol(bmp, kind, &po, out, err);
            }
            break;
        }
        default:
            return fail(err, "format requires a Scene (not available for this object)",
                        TC_ERR_UNSUPPORTED);
    }
    return TC_OK;
}

inline tc_output_opts output_defaults(const tc_output_opts* o) noexcept {
    return o ? *o : tc_output_opts{};
}

inline tc_status render_scene_vector(const scene::Scene& sc, int fmt, std::string& out,
                                     tc_error_handle* err) {
    backend::vector::VectorOptions vo;
    vo.font_size = 1.0;
    switch (fmt) {
        case TC_OUT_SVG: {
            backend::vector::SvgBackend b;
            b.set_options(vo);
            out = backend::vector::render_vector(b, sc);
            break;
        }
        case TC_OUT_PDF: {
            backend::vector::PdfBackend b;
            out = backend::vector::render_pdf(b, sc);
            break;
        }
        case TC_OUT_EPS: {
            backend::vector::EpsBackend b;
            out = backend::vector::render_eps(b, sc);
            break;
        }
        default:
            return fail(err, "not a vector format", TC_ERR_INVALID_ARG);
    }
    return TC_OK;
}

inline math3d::Vec3 vec3_at(const std::vector<double>& v, std::size_t i) noexcept {
    return math3d::Vec3{v[i], v[i + 1], v[i + 2]};
}

inline math4d::Vec4 vec4_at(const std::vector<double>& v, std::size_t i) noexcept {
    return math4d::Vec4{v[i], v[i + 1], v[i + 2], v[i + 3]};
}

inline scene::Scene build_3d(const Scene3dHandle& h, std::uint32_t w, std::uint32_t rows) {
    if (h.kind == "scatter3d") {
        chart3d::Scatter3dOpts o;
        o.cols = static_cast<double>(w);
        o.rows = static_cast<double>(rows);
        o.azimuth_deg = h.azimuth;
        o.elevation_deg = h.elevation;
        o.fovy_deg = h.fovy;
        std::vector<math3d::Vec3> pts;
        pts.reserve(h.points.size() / 3);
        for (std::size_t i = 0; i + 2 < h.points.size(); i += 3) pts.push_back(vec3_at(h.points, i));
        return chart3d::scatter3d(pts, o);
    }
    if (h.kind == "surface3d") {
        chart3d::Surface3dOpts o;
        o.cols = static_cast<double>(w);
        o.rows = static_cast<double>(rows);
        o.azimuth_deg = h.azimuth;
        o.elevation_deg = h.elevation;
        o.fovy_deg = h.fovy;
        o.res_x = static_cast<int>(h.grid_cols);
        o.res_y = static_cast<int>(h.grid_rows);
        const std::uint32_t nx = h.grid_cols, ny = h.grid_rows;
        const double dx = (nx > 1) ? (h.x1 - h.x0) / static_cast<double>(nx - 1) : 0.0;
        const double dy = (ny > 1) ? (h.y1 - h.y0) / static_cast<double>(ny - 1) : 0.0;
        const auto z_at = [&h, nx, ny](std::uint32_t cx, std::uint32_t cy) {
            const std::uint32_t c = cx < nx ? cx : nx - 1;
            const std::uint32_t r = cy < ny ? cy : ny - 1;
            return h.grid[static_cast<std::size_t>(r) * nx + c];
        };
        const auto f = [&](double x, double y) {
            const double gx = dx > 0.0 ? (x - h.x0) / dx : 0.0;
            const double gy = dy > 0.0 ? (y - h.y0) / dy : 0.0;
            const auto ix = static_cast<std::uint32_t>(std::max(0.0, std::min(gx, static_cast<double>(nx - 1))));
            const auto iy = static_cast<std::uint32_t>(std::max(0.0, std::min(gy, static_cast<double>(ny - 1))));
            const std::uint32_t jx = std::min(ix + 1u, nx - 1u);
            const std::uint32_t jy = std::min(iy + 1u, ny - 1u);
            const double tx = std::max(0.0, std::min(1.0, gx - static_cast<double>(ix)));
            const double ty = std::max(0.0, std::min(1.0, gy - static_cast<double>(iy)));
            const double a = z_at(ix, iy) * (1.0 - tx) + z_at(jx, iy) * tx;
            const double b = z_at(ix, jy) * (1.0 - tx) + z_at(jx, jy) * tx;
            return a * (1.0 - ty) + b * ty;
        };
        return chart3d::surface3d(f, h.x0, h.x1, h.y0, h.y1, o);
    }
    chart4d::Wire4dOpts wo;
    wo.cols = static_cast<double>(w);
    wo.rows = static_cast<double>(rows);
    std::vector<math4d::Vec4> verts;
    verts.reserve(h.points.size() / 4);
    for (std::size_t i = 0; i + 3 < h.points.size(); i += 4) verts.push_back(vec4_at(h.points, i));
    std::vector<chart4d::Edge> edges;
    if (h.edges.empty()) {
        for (std::uint32_t i = 0; i + 1 < verts.size(); ++i) edges.emplace_back(i, i + 1);
    } else {
        for (std::size_t i = 0; i + 1 < h.edges.size(); i += 2)
            edges.emplace_back(h.edges[i], h.edges[i + 1]);
    }
    math3d::Camera cam3;
    cam3.orbit(h.azimuth, h.elevation, 4.0);
    cam3.fovy_deg = h.fovy;
    math4d::Camera4 cam4;
    cam4.focal = math4d::focal_from_fov_deg(h.fovy);
    const math4d::Mat5 rot = math4d::rotate_xw(h.rot_xw * math3d::kDeg2Rad) *
                             math4d::rotate_yz(h.rot_yz * math3d::kDeg2Rad);
    const chart4d::Projector4D proj(rot, cam4, std::make_shared<chart3d::Projector3D>(cam3));
    return chart4d::wire4d(verts, edges, wo, proj);
}

inline tc_key key_to_c(interaction::Key k) noexcept {
    return static_cast<tc_key>(static_cast<int>(k));  // 枚举顺序与 tc_key 一致
}

inline std::uint8_t mods_to_c(interaction::ModMask m) noexcept {
    std::uint8_t out = 0;
    if (m & interaction::mod::kShift) out |= 1u;
    if (m & interaction::mod::kAlt) out |= 2u;
    if (m & interaction::mod::kCtrl) out |= 4u;
    return out;
}

inline tc_event event_to_c(const interaction::Event& ev, const std::string* paste) noexcept {
    tc_event out{};
    out.kind = static_cast<std::uint8_t>(interaction::kind_of(ev));
    if (const auto* k = std::get_if<interaction::KeyEvent>(&ev)) {
        out.key = static_cast<std::uint8_t>(key_to_c(k->key));
        out.codepoint = static_cast<std::uint32_t>(k->ch);
        out.mods = mods_to_c(k->mods);
    } else if (const auto* m = std::get_if<interaction::MouseEvent>(&ev)) {
        out.button = static_cast<std::uint8_t>(m->button);
        out.action = static_cast<std::uint8_t>(m->action);
        out.x = m->x;
        out.y = m->y;
        out.mods = mods_to_c(m->mods);
    } else if (const auto* r = std::get_if<interaction::ResizeEvent>(&ev)) {
        out.cols = r->cols;
        out.rows = r->rows;
    } else if (const auto* f = std::get_if<interaction::FocusEvent>(&ev)) {
        out.focused = f->focused ? 1u : 0u;
    } else if (paste) {
        out.text = paste->c_str();
        out.text_len = static_cast<std::uint64_t>(paste->size());
    } else {
        out.kind = static_cast<std::uint8_t>(TC_EVENT_UNKNOWN);
    }
    return out;
}

struct DataHandle {
    data::DataFrame frame;
};

inline DataHandle* as_data(tc_data_handle h) noexcept {
    return reinterpret_cast<DataHandle*>(h);
}

}  // namespace

extern "C" {

// ── 终端 ────────────────────────────────────────────────────────────────────

tc_term_opts tc_term_opts_default(void) {
    tc_term_opts o{};
    const auto d = term::TerminalOptions{};
    o.raw_mode = d.raw_mode ? 1u : 0u;
    o.alt_screen = d.alt_screen ? 1u : 0u;
    o.hide_cursor = d.hide_cursor ? 1u : 0u;
    o.mouse = d.enable_mouse ? 1u : 0u;
    return o;
}

tc_term_handle tc_term_create(const tc_term_opts* o, tc_error_handle* err) {
    term::TerminalOptions opt;
    if (o) {
        opt.raw_mode = o->raw_mode != 0;
        opt.alt_screen = o->alt_screen != 0;
        opt.hide_cursor = o->hide_cursor != 0;
        opt.enable_mouse = o->mouse != 0;
    }
    auto* h = new (std::nothrow) TermHandle(opt);
    if (!h) {
        set_err(err, "out of memory");
        return nullptr;
    }
    const Status s = h->t.enter();
    if (!is_ok(s)) {
        set_err(err, std::string("terminal enter failed (") + describe(s) + ")");
        delete h;
        return nullptr;
    }
    return reinterpret_cast<tc_term_handle>(h);
}

void tc_term_free(tc_term_handle term) {
    auto* h = reinterpret_cast<TermHandle*>(term);
    if (h) {
        h->t.leave();
        delete h;
    }
}

tc_status tc_term_size_query(tc_term_size* out, tc_error_handle* err) {
    if (!out) return fail(err, "null out", TC_ERR_INVALID_ARG);
    if (!term::Terminal::stdout_is_tty())
        return fail(err, "stdout is not a tty", TC_ERR_UNSUPPORTED);
    const term::TerminalSize s = term::query_size();
    out->cols = s.cols;
    out->rows = s.rows;
    return TC_OK;
}

tc_term_size tc_term_size_from_env(void) {
    const term::TerminalSize s = term::size_from_env();
    // C 面契约：环境变量缺失时给 80×24，不把 0 尺寸漏给调用方。
    return tc_term_size{static_cast<std::uint16_t>(s.cols ? s.cols : 80u),
                        static_cast<std::uint16_t>(s.rows ? s.rows : 24u)};
}

tc_status tc_term_caps_detect(tc_term_caps* out, uint8_t force_tty) {
    if (!out) return TC_ERR_INVALID_ARG;
    term::CapEnvironment env;
    if (const char* p = std::getenv("TERM")) env.term = p;
    if (const char* p = std::getenv("COLORTERM")) env.colorterm = p;
    if (const char* p = std::getenv("TERM_PROGRAM")) env.term_program = p;
    if (const char* p = std::getenv("WT_SESSION")) env.wt_session = p;
    if (std::getenv("CI") || std::getenv("GITHUB_ACTIONS")) env.ci = true;
    const bool tty = force_tty ? true : term::Terminal::stdout_is_tty();
    const term::TerminalCaps c = term::detect(env, tty);
    out->unicode = c.unicode ? 1u : 0u;
    out->color_depth = static_cast<std::uint8_t>(c.color_depth);
    out->mouse = c.mouse ? 1u : 0u;
    out->alt_screen = c.alt_screen ? 1u : 0u;
    out->title_setting = c.title_setting ? 1u : 0u;
    return TC_OK;
}

uint8_t tc_term_stdout_is_tty(void) { return term::Terminal::stdout_is_tty() ? 1u : 0u; }
uint8_t tc_term_stdin_is_tty(void) { return term::Terminal::stdin_is_tty() ? 1u : 0u; }

tc_status tc_term_clipboard_set(const char* text, tc_error_handle* err) {
    if (!text) return fail(err, "null text", TC_ERR_INVALID_ARG);
    const std::string s = term::clipboard_set(text);
    return tc_write_stdout(s.data(), static_cast<std::uint64_t>(s.size()), err);
}

tc_status tc_term_clipboard_set_primary(const char* text, tc_error_handle* err) {
    if (!text) return fail(err, "null text", TC_ERR_INVALID_ARG);
    const std::string s = term::clipboard_set(text, term::ClipboardTarget::Primary);
    return tc_write_stdout(s.data(), static_cast<std::uint64_t>(s.size()), err);
}

tc_status tc_term_hyperlink(const char* uri, const char* id,
                            const char* text, tc_error_handle* err) {
    if (!uri || !text) return fail(err, "null uri or text", TC_ERR_INVALID_ARG);
    const std::string s = term::hyperlink(text, uri, id ? id : "");
    return tc_write_stdout(s.data(), static_cast<std::uint64_t>(s.size()), err);
}

tc_status tc_term_hyperlink_close(tc_error_handle* err) {
    const std::string s = term::hyperlink_close();
    return tc_write_stdout(s.data(), static_cast<std::uint64_t>(s.size()), err);
}

tc_status tc_term_mouse(uint8_t enable, uint16_t mode, uint16_t encoding, tc_error_handle* err) {
    const std::string s = enable ? term::enable_mouse(static_cast<term::MouseMode>(mode),
                                                     static_cast<term::MouseEncoding>(encoding))
                                 : term::disable_all_mouse();
    return tc_write_stdout(s.data(), static_cast<std::uint64_t>(s.size()), err);
}

// ── 自绘画布 / 增量渲染 ─────────────────────────────────────────────────────

tc_canvas_handle tc_canvas_create(uint32_t width, uint32_t height, tc_error_handle* err) {
    if (!width || !height) {
        set_err(err, "canvas width/height must be non-zero");
        return nullptr;
    }
    auto* h = new (std::nothrow) CanvasHandle(width, height);
    if (!h) set_err(err, "out of memory");
    return reinterpret_cast<tc_canvas_handle>(h);
}

void tc_canvas_free(tc_canvas_handle canvas) { delete reinterpret_cast<CanvasHandle*>(canvas); }

int32_t tc_canvas_resize(tc_canvas_handle canvas, uint32_t width, uint32_t height) {
    auto* h = reinterpret_cast<CanvasHandle*>(canvas);
    if (!h || !width || !height) return -1;
    h->cv.resize(width, height);
    return 0;
}

uint32_t tc_canvas_width(tc_canvas_handle canvas) {
    auto* h = reinterpret_cast<CanvasHandle*>(canvas);
    return h ? h->cv.to_buffer().width() : 0u;
}

uint32_t tc_canvas_height(tc_canvas_handle canvas) {
    auto* h = reinterpret_cast<CanvasHandle*>(canvas);
    return h ? h->cv.to_buffer().height() : 0u;
}

void tc_canvas_clear(tc_canvas_handle canvas) {
    if (auto* h = reinterpret_cast<CanvasHandle*>(canvas)) h->cv.clear();
}

uint8_t tc_canvas_put(tc_canvas_handle canvas, int32_t x, int32_t y,
                      uint32_t codepoint, const tc_style* style) {
    auto* h = reinterpret_cast<CanvasHandle*>(canvas);
    if (!h || x < 0 || y < 0 || codepoint > 0x10FFFF) return 0u;
    const bool ok = h->cv.put(Point{static_cast<double>(x), static_cast<double>(y)},
                              static_cast<char32_t>(codepoint), style_or_default(style));
    return ok ? 1u : 0u;
}

uint8_t tc_canvas_text(tc_canvas_handle canvas, int32_t x, int32_t y,
                       const char* utf8, const tc_style* style) {
    auto* h = reinterpret_cast<CanvasHandle*>(canvas);
    if (!h || !utf8 || x < 0 || y < 0) return 0u;
    h->cv.draw_text(Point{static_cast<double>(x), static_cast<double>(y)},
                    utf8, style_or_default(style));
    return 1u;
}

uint8_t tc_canvas_line(tc_canvas_handle canvas,
                       int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                       uint32_t codepoint, const tc_style* style) {
    auto* h = reinterpret_cast<CanvasHandle*>(canvas);
    if (!h || codepoint > 0x10FFFF) return 0u;
    h->cv.draw_line(Point{static_cast<double>(x0), static_cast<double>(y0)},
                    Point{static_cast<double>(x1), static_cast<double>(y1)},
                    static_cast<char32_t>(codepoint), style_or_default(style));
    return 1u;
}

void tc_canvas_fill_rect(tc_canvas_handle canvas,
                         int32_t x, int32_t y, uint32_t w, uint32_t hgt,
                         uint32_t codepoint, const tc_style* style) {
    auto* h = reinterpret_cast<CanvasHandle*>(canvas);
    if (!h || codepoint > 0x10FFFF) return;
    h->cv.fill_rect(Rect{static_cast<double>(x), static_cast<double>(y),
                         static_cast<double>(w), static_cast<double>(hgt)},
                    static_cast<char32_t>(codepoint), style_or_default(style));
}

tc_output_opts tc_output_opts_default(void) {
    tc_output_opts o{};
    o.format = TC_OUT_ANSI;
    o.color_depth = TC_DEPTH_TRUECOLOR;
    o.protocol = TC_PROTO_AUTO;
    return o;
}

tc_status tc_canvas_render(tc_canvas_handle canvas, const tc_output_opts* out_opts,
                           char* buf, uint64_t cap, uint64_t* out_len, tc_error_handle* err) {
    auto* h = reinterpret_cast<CanvasHandle*>(canvas);
    if (!h) return fail(err, "null canvas", TC_ERR_INVALID_HANDLE);
    if (!out_len) return fail(err, "null out_len", TC_ERR_INVALID_ARG);
    std::string out;
    const tc_status st = render_cells_to_output(h->cv.to_buffer(), out_opts, out, err);
    if (st != TC_OK) return st;
    return emit_bytes(out, buf, cap, out_len, err);
}

tc_renderer_handle tc_renderer_create(uint8_t color_depth, tc_error_handle* err) {
    auto* h = new (std::nothrow) RendererHandle(depth_of(color_depth));
    if (!h) set_err(err, "out of memory");
    return reinterpret_cast<tc_renderer_handle>(h);
}

void tc_renderer_free(tc_renderer_handle renderer) {
    delete reinterpret_cast<RendererHandle*>(renderer);
}

void tc_renderer_reset(tc_renderer_handle renderer) {
    if (auto* h = reinterpret_cast<RendererHandle*>(renderer)) {
        h->r.reset();
        h->cached.clear();
        h->has_cache = false;
    }
}

tc_status tc_renderer_frame(tc_renderer_handle renderer, tc_canvas_handle canvas,
                            char* buf, uint64_t cap, uint64_t* out_len, tc_error_handle* err) {
    auto* rh = reinterpret_cast<RendererHandle*>(renderer);
    auto* ch = reinterpret_cast<CanvasHandle*>(canvas);
    if (!rh || !ch) return fail(err, "null renderer or canvas", TC_ERR_INVALID_HANDLE);
    if (!out_len) return fail(err, "null out_len", TC_ERR_INVALID_ARG);
    if (!buf || cap == 0) {
        rh->cached = rh->r.render(ch->cv.to_buffer());
        rh->has_cache = true;
        *out_len = static_cast<std::uint64_t>(rh->cached.size());
        return TC_OK;
    }
    std::string out;
    if (rh->has_cache) {
        out = std::move(rh->cached);
        rh->has_cache = false;
    } else {
        out = rh->r.render(ch->cv.to_buffer());
    }
    return emit_bytes(out, buf, cap, out_len, err);
}

// ── 输入解析 ────────────────────────────────────────────────────────────────

tc_input_handle tc_input_create(tc_error_handle* err) {
    auto* h = new (std::nothrow) InputHandle();
    if (!h) set_err(err, "out of memory");
    return reinterpret_cast<tc_input_handle>(h);
}

void tc_input_free(tc_input_handle input) { delete reinterpret_cast<InputHandle*>(input); }

void tc_input_reset(tc_input_handle input) {
    if (auto* h = reinterpret_cast<InputHandle*>(input)) {
        h->parser.reset();
        h->pastes.clear();
        h->cached_events.clear();
        h->cached_pastes.clear();
        h->has_cache = false;
    }
}

tc_status tc_input_feed(tc_input_handle input, const uint8_t* bytes, uint64_t n,
                        tc_event* out, uint64_t cap, uint64_t* out_count, tc_error_handle* err) {
    auto* h = reinterpret_cast<InputHandle*>(input);
    if (!h || !out_count) return fail(err, "null input or out_count", TC_ERR_INVALID_HANDLE);
    if (n && !bytes) return fail(err, "null bytes", TC_ERR_INVALID_ARG);
    if (!h->has_cache) {
        h->cached_events = h->parser.feed(
            std::string_view(reinterpret_cast<const char*>(bytes), static_cast<std::size_t>(n)));
        h->cached_pastes.clear();
        h->cached_pastes.reserve(h->cached_events.size());
        for (const auto& ev : h->cached_events) {
            if (const auto* p = std::get_if<interaction::PasteEvent>(&ev))
                h->cached_pastes.push_back(p->text);
        }
        h->pastes = h->cached_pastes;
        h->has_cache = true;
    }
    const std::vector<interaction::Event>& events = h->cached_events;
    const std::vector<std::string>& pastes = h->pastes;
    const std::uint64_t total = static_cast<std::uint64_t>(events.size());
    *out_count = total;
    if (!out || cap == 0) return TC_OK;
    const std::uint64_t k = std::min<std::uint64_t>(total, cap);
    std::size_t paste_i = 0;
    for (std::uint64_t i = 0; i < k; ++i) {
        const std::string* paste = nullptr;
        if (interaction::kind_of(events[static_cast<std::size_t>(i)]) ==
            interaction::EventKind::Paste) {
            paste = &pastes[paste_i++];
        }
        out[i] = event_to_c(events[static_cast<std::size_t>(i)], paste);
    }
    h->has_cache = false;
    h->cached_events.clear();
    h->cached_pastes.clear();
    return TC_OK;
}

// ── 命中区域 ────────────────────────────────────────────────────────────────

tc_regions_handle tc_regions_create(tc_error_handle* err) {
    auto* h = new (std::nothrow) RegionsHandle();
    if (!h) set_err(err, "out of memory");
    return reinterpret_cast<tc_regions_handle>(h);
}

void tc_regions_free(tc_regions_handle regions) { delete reinterpret_cast<RegionsHandle*>(regions); }

void tc_regions_clear(tc_regions_handle regions) {
    if (auto* h = reinterpret_cast<RegionsHandle*>(regions)) h->regions.clear();
}

uint64_t tc_regions_count(tc_regions_handle regions) {
    auto* h = reinterpret_cast<RegionsHandle*>(regions);
    return h ? static_cast<std::uint64_t>(h->regions.size()) : 0u;
}

uint64_t tc_regions_add_rect(tc_regions_handle regions,
                             int32_t x, int32_t y, uint32_t w, uint32_t hgt, uint32_t payload) {
    auto* r = reinterpret_cast<RegionsHandle*>(regions);
    if (!r) return 0u;
    r->regions.push_back(chart::HitRegion::rect(
        Rect{static_cast<double>(x), static_cast<double>(y), static_cast<double>(w),
             static_cast<double>(hgt)}, payload));
    return static_cast<std::uint64_t>(r->regions.size());
}

uint64_t tc_regions_add_circle(tc_regions_handle regions,
                               int32_t cx, int32_t cy, double radius, uint32_t payload) {
    auto* r = reinterpret_cast<RegionsHandle*>(regions);
    if (!r) return 0u;
    r->regions.push_back(chart::HitRegion::circle(
        Point{static_cast<double>(cx), static_cast<double>(cy)}, radius, payload));
    return static_cast<std::uint64_t>(r->regions.size());
}

uint64_t tc_regions_add_polygon(tc_regions_handle regions, const int32_t* xy,
                                uint64_t point_count, uint32_t payload, tc_error_handle* err) {
    auto* r = reinterpret_cast<RegionsHandle*>(regions);
    if (!r) {
        set_err(err, "null regions");
        return 0u;
    }
    if (!xy || point_count < 3 || point_count > 0xFFFF) {
        set_err(err, "polygon needs 3..65535 points");
        return 0u;
    }
    std::vector<Point> pts;
    pts.reserve(static_cast<std::size_t>(point_count));
    for (std::uint64_t i = 0; i < point_count; ++i) {
        pts.push_back(Point{static_cast<double>(xy[i * 2]), static_cast<double>(xy[i * 2 + 1])});
    }
    r->regions.push_back(chart::HitRegion::polygon(std::move(pts), payload));
    return static_cast<std::uint64_t>(r->regions.size());
}

tc_status tc_regions_hit(tc_regions_handle regions, double x, double y,
                         uint32_t* payloads, uint64_t cap, uint64_t* out_count) {
    auto* r = reinterpret_cast<RegionsHandle*>(regions);
    if (!r || !out_count) return TC_ERR_INVALID_HANDLE;
    const std::vector<std::uint32_t> hits = interaction::hit_payloads(r->regions, x, y);
    const std::uint64_t total = static_cast<std::uint64_t>(hits.size());
    *out_count = total;
    if (!payloads || cap == 0) return TC_OK;
    const std::uint64_t k = std::min<std::uint64_t>(total, cap);
    for (std::uint64_t i = 0; i < k; ++i) payloads[i] = hits[static_cast<std::size_t>(i)];
    return TC_OK;
}

// ── 导出物种 ────────────────────────────────────────────────────────────────

tc_status tc_chart_export(tc_chart_handle chart, const tc_output_opts* out_opts,
                          const tc_plot_opts* plot_opts, char* buf, uint64_t cap,
                          uint64_t* out_len, tc_error_handle* err) {
    auto* c = reinterpret_cast<ChartData*>(chart);
    if (!c) return fail(err, "null chart", TC_ERR_INVALID_HANDLE);
    if (!out_len) return fail(err, "null out_len", TC_ERR_INVALID_ARG);
    if (!has_data(*c)) return fail(err, "empty chart data", TC_ERR_INVALID_ARG);
    const tc_output_opts o = output_defaults(out_opts);
    std::string out;

    if (o.format == TC_OUT_SVG || o.format == TC_OUT_PDF || o.format == TC_OUT_EPS) {
        const std::uint32_t w = (plot_opts && plot_opts->width) ? plot_opts->width : 80u;
        const std::uint32_t rows = (plot_opts && plot_opts->height) ? plot_opts->height : 24u;
        chart::Chart& ch = apply_display(*c, plot_opts);
        auto sc = api::chart_to_scene(ch, w, rows);
        if (!sc.has_value())
            return fail(err, std::string("chart_to_scene failed (") + describe(sc.error()) + ")",
                        TC_ERR_RENDER);
        scene::Scene scene = std::move(sc.value());
        decorate_scene_labels(scene, *c);
        const tc_status st = render_scene_vector(scene, static_cast<int>(o.format), out, err);
        if (st != TC_OK) return st;
    } else if (o.format == TC_OUT_ANSI) {
        const tc_status st = render_chart_bytes(*c, plot_opts, out, err);
        if (st != TC_OK) return st;
    } else {
        CellBuffer cells;
        if (!build_plot_with_labels(*c, plot_opts, cells))
            return fail(err, "invalid width/height", TC_ERR_INVALID_ARG);
        tc_output_opts effective = o;
        if (effective.color_depth == 0) effective.color_depth = TC_DEPTH_TRUECOLOR;
        const tc_status st = render_cells_to_output(cells, &effective, out, err);
        if (st != TC_OK) return st;
    }
    return emit_bytes(out, buf, cap, out_len, err);
}

tc_image_handle tc_chart_to_image(tc_chart_handle chart, const tc_plot_opts* opts,
                                  tc_error_handle* err) {
    auto* c = reinterpret_cast<ChartData*>(chart);
    if (!c) {
        set_err(err, "null chart");
        return nullptr;
    }
    if (!has_data(*c)) {
        set_err(err, "empty chart data");
        return nullptr;
    }
    CellBuffer cells;
    if (!build_plot_with_labels(*c, opts, cells)) {
        set_err(err, "invalid width/height");
        return nullptr;
    }
    gui::CellBitmapOptions co{};
    auto* bmp = new (std::nothrow) image::Bitmap(gui::render_cells_to_bitmap(cells, co));
    if (!bmp) set_err(err, "out of memory");
    return reinterpret_cast<tc_image_handle>(bmp);
}

// ── 时序图 ──────────────────────────────────────────────────────────────────

tc_seq_handle tc_seq_from_text(const char* text, tc_error_handle* err) {
    if (!text) {
        set_err(err, "null text");
        return nullptr;
    }
    dsl::ParseDiagnostic diag;
    auto parsed = dsl::parse_sequence(text, {}, &diag);
    if (!parsed.has_value()) {
        set_err(err, std::string("parse sequence failed (") + describe(parsed.error()) + ")");
        return nullptr;
    }
    const auto& ast = parsed.value();
    auto* h = new (std::nothrow) SeqHandle();
    if (!h) {
        set_err(err, "out of memory");
        return nullptr;
    }
    h->model.participants.reserve(ast.participants.size());
    for (const auto& p : ast.participants) {
        h->model.participants.push_back(graph::SeqParticipant{p.key, p.label, false});
    }
    h->model.messages.reserve(ast.messages.size());
    for (const auto& m : ast.messages) {
        graph::SeqArrowKind kind = graph::SeqArrowKind::Solid;
        switch (m.kind) {
            case dsl::MessageKind::Solid: kind = graph::SeqArrowKind::Solid; break;
            case dsl::MessageKind::Dotted: kind = graph::SeqArrowKind::Dotted; break;
            case dsl::MessageKind::Async:
            case dsl::MessageKind::Open: kind = graph::SeqArrowKind::Open; break;
            case dsl::MessageKind::AsyncDotted:
            case dsl::MessageKind::OpenDotted: kind = graph::SeqArrowKind::OpenDotted; break;
            case dsl::MessageKind::Cross: kind = graph::SeqArrowKind::Cross; break;
            case dsl::MessageKind::CrossDotted: kind = graph::SeqArrowKind::CrossDotted; break;
        }
        h->model.messages.push_back(
            graph::SeqMessage{m.from, m.to, m.text, kind, m.activate_target, m.deactivate_source});
    }
    h->model.notes.reserve(ast.notes.size());
    for (const auto& n : ast.notes) {
        graph::SeqNote sn;
        sn.text = n.text;
        sn.placement = n.placement == dsl::SequenceNote::Placement::Left
                           ? graph::SeqNote::Placement::Left
                           : (n.placement == dsl::SequenceNote::Placement::Right
                                  ? graph::SeqNote::Placement::Right
                                  : graph::SeqNote::Placement::Over);
        if (!n.participants.empty()) sn.anchor = n.participants[0];
        if (n.participants.size() > 1) sn.anchor_b = n.participants[1];
        sn.after = static_cast<std::size_t>(-1);
        h->model.notes.push_back(std::move(sn));
    }
    return reinterpret_cast<tc_seq_handle>(h);
}

void tc_seq_free(tc_seq_handle seq) { delete reinterpret_cast<SeqHandle*>(seq); }

uint64_t tc_seq_participant_count(tc_seq_handle seq) {
    auto* h = reinterpret_cast<SeqHandle*>(seq);
    return h ? static_cast<std::uint64_t>(h->model.participants.size()) : 0u;
}

uint64_t tc_seq_message_count(tc_seq_handle seq) {
    auto* h = reinterpret_cast<SeqHandle*>(seq);
    return h ? static_cast<std::uint64_t>(h->model.messages.size()) : 0u;
}

tc_status tc_seq_render(tc_seq_handle seq, const tc_output_opts* out_opts,
                        char* buf, uint64_t cap, uint64_t* out_len, tc_error_handle* err) {
    auto* h = reinterpret_cast<SeqHandle*>(seq);
    if (!h) return fail(err, "null seq", TC_ERR_INVALID_HANDLE);
    if (!out_len) return fail(err, "null out_len", TC_ERR_INVALID_ARG);
    const tc_output_opts o = output_defaults(out_opts);
    const std::uint32_t w = o.width ? o.width : 80u;
    const std::uint32_t rows = o.height ? o.height : 24u;
    canvas::Canvas cv(w, rows);
    (void)graph::render_uml_sequence(h->model, cv);
    std::string out;
    const tc_status st = render_cells_to_output(cv.to_buffer(), out_opts, out, err);
    if (st != TC_OK) return st;
    return emit_bytes(out, buf, cap, out_len, err);
}

// ── 多帧编码 / 富文本 ───────────────────────────────────────────────────────

tc_status tc_image_encode_frames(const tc_image_handle* frames, uint64_t count,
                                 const char* format, uint8_t quality, uint32_t fps,
                                 char* buf, uint64_t cap, uint64_t* out_len, tc_error_handle* err) {
    if (!out_len) return fail(err, "null out_len", TC_ERR_INVALID_ARG);
    if (!frames || !count) return fail(err, "no frames", TC_ERR_INVALID_ARG);
    if (!format) return fail(err, "null format", TC_ERR_INVALID_ARG);
    const std::string fmt = lower_ascii(format);
    if (!media::has_encoder(fmt) || !media::is_multiframe(fmt)) {
        return fail(err, "unsupported frame format: " + fmt, TC_ERR_UNSUPPORTED);
    }
    if (count > 0xFFFF) return fail(err, "too many frames", TC_ERR_INVALID_ARG);
    std::vector<image::Bitmap> bmps;
    bmps.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        auto* b = as_bitmap(frames[i]);
        if (!b || b->empty()) return fail(err, "null/empty frame", TC_ERR_INVALID_ARG);
        bmps.push_back(*b);
    }
    image::EncodeOptions eo;
    eo.quality = quality ? static_cast<int>(quality) : 85;
    eo.fps = fps ? static_cast<int>(fps) : 25;
    const std::string out = media::encode(bmps, fmt, eo);
    if (out.empty()) return fail(err, "frame encode failed", TC_ERR_RENDER);
    return emit_bytes(out, buf, cap, out_len, err);
}

tc_status tc_doc_add_rich_paragraph(tc_doc_handle doc, const tc_span* spans, uint64_t count,
                                    tc_error_handle* err) {
    auto* d = as_doc(doc);
    if (!d) return fail(err, "null doc", TC_ERR_INVALID_HANDLE);
    if (!spans || !count || count > 0xFFFF) return fail(err, "invalid spans", TC_ERR_INVALID_ARG);
    std::vector<document::Span> ss;
    ss.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        if (!spans[i].text) return fail(err, "null span text", TC_ERR_INVALID_ARG);
        ss.emplace_back(std::string(spans[i].text), to_style(&spans[i].style));
    }
    d->doc.add_block(document::Block::paragraph(std::move(ss)));
    return TC_OK;
}

// ── 3D / 4D ─────────────────────────────────────────────────────────────────

tc_scene3d_handle tc_scene3d_create(const char* kind, tc_error_handle* err) {
    if (!kind) {
        set_err(err, "null kind");
        return nullptr;
    }
    const std::string k = kind;
    if (k != "scatter3d" && k != "surface3d" && k != "wire4d") {
        set_err(err, "unknown 3d kind: " + k);
        return nullptr;
    }
    auto* h = new (std::nothrow) Scene3dHandle();
    if (!h) {
        set_err(err, "out of memory");
        return nullptr;
    }
    h->kind = k;
    return reinterpret_cast<tc_scene3d_handle>(h);
}

void tc_scene3d_free(tc_scene3d_handle scene) { delete reinterpret_cast<Scene3dHandle*>(scene); }

tc_status tc_scene3d_set_points(tc_scene3d_handle scene, const double* coords,
                                uint64_t count, tc_error_handle* err) {
    auto* h = reinterpret_cast<Scene3dHandle*>(scene);
    if (!h) return fail(err, "null scene", TC_ERR_INVALID_HANDLE);
    const std::uint64_t stride = (h->kind == "wire4d") ? 4u : 3u;
    if (!coords || !count || count % stride != 0)
        return fail(err, "point count must be a multiple of the coordinate stride",
                    TC_ERR_INVALID_ARG);
    h->points.assign(coords, coords + static_cast<std::size_t>(count));
    return TC_OK;
}

tc_status tc_scene3d_set_grid(tc_scene3d_handle scene, const double* z,
                              uint32_t cols, uint32_t rows, tc_error_handle* err) {
    auto* h = reinterpret_cast<Scene3dHandle*>(scene);
    if (!h) return fail(err, "null scene", TC_ERR_INVALID_HANDLE);
    if (h->kind != "surface3d")
        return fail(err, "set_grid only applies to surface3d", TC_ERR_UNSUPPORTED);
    if (!z || !cols || !rows || static_cast<std::uint64_t>(cols) * rows > 0x1000000)
        return fail(err, "invalid grid", TC_ERR_INVALID_ARG);
    h->grid.assign(z, z + static_cast<std::size_t>(cols) * rows);
    h->grid_cols = cols;
    h->grid_rows = rows;
    return TC_OK;
}

tc_status tc_scene3d_set_edges(tc_scene3d_handle scene, const uint32_t* pairs,
                               uint64_t count, tc_error_handle* err) {
    auto* h = reinterpret_cast<Scene3dHandle*>(scene);
    if (!h) return fail(err, "null scene", TC_ERR_INVALID_HANDLE);
    if (h->kind != "wire4d")
        return fail(err, "set_edges only applies to wire4d", TC_ERR_UNSUPPORTED);
    if (!pairs || !count || count % 2 != 0)
        return fail(err, "edges must be index pairs", TC_ERR_INVALID_ARG);
    h->edges.assign(pairs, pairs + static_cast<std::size_t>(count));
    return TC_OK;
}

tc_status tc_scene3d_set_view(tc_scene3d_handle scene,
                              double azimuth_deg, double elevation_deg, double fovy_deg) {
    auto* h = reinterpret_cast<Scene3dHandle*>(scene);
    if (!h) return TC_ERR_INVALID_HANDLE;
    if (!std::isfinite(azimuth_deg) || !std::isfinite(elevation_deg) ||
        !std::isfinite(fovy_deg) || fovy_deg <= 0.0 || fovy_deg >= 180.0)
        return TC_ERR_INVALID_ARG;
    h->azimuth = azimuth_deg;
    h->elevation = elevation_deg;
    h->fovy = fovy_deg;
    return TC_OK;
}

tc_status tc_scene3d_set_range(tc_scene3d_handle scene, uint8_t axis,
                               double lo, double hi, tc_error_handle* err) {
    auto* h = reinterpret_cast<Scene3dHandle*>(scene);
    if (!h) return fail(err, "null scene", TC_ERR_INVALID_HANDLE);
    if (h->kind != "surface3d")
        return fail(err, "only surface3d has x/y ranges", TC_ERR_UNSUPPORTED);
    if (axis > 1 || !std::isfinite(lo) || !std::isfinite(hi) || lo >= hi)
        return fail(err, "invalid range", TC_ERR_INVALID_ARG);
    if (axis == 0) {
        h->x0 = lo;
        h->x1 = hi;
    } else {
        h->y0 = lo;
        h->y1 = hi;
    }
    return TC_OK;
}

tc_status tc_scene3d_set_rotation4d(tc_scene3d_handle scene, double xw_deg, double yz_deg) {
    auto* h = reinterpret_cast<Scene3dHandle*>(scene);
    if (!h) return TC_ERR_INVALID_HANDLE;
    if (h->kind != "wire4d" || !std::isfinite(xw_deg) || !std::isfinite(yz_deg))
        return TC_ERR_INVALID_ARG;
    h->rot_xw = xw_deg;
    h->rot_yz = yz_deg;
    return TC_OK;
}

tc_status tc_scene3d_render(tc_scene3d_handle scene, const tc_output_opts* out_opts,
                            char* buf, uint64_t cap, uint64_t* out_len, tc_error_handle* err) {
    auto* h = reinterpret_cast<Scene3dHandle*>(scene);
    if (!h) return fail(err, "null scene", TC_ERR_INVALID_HANDLE);
    if (!out_len) return fail(err, "null out_len", TC_ERR_INVALID_ARG);
    if (h->kind == "surface3d" && h->grid.empty())
        return fail(err, "surface3d needs set_grid", TC_ERR_INVALID_ARG);
    if (h->kind != "surface3d" && h->points.empty())
        return fail(err, "need set_points", TC_ERR_INVALID_ARG);
    const tc_output_opts o = output_defaults(out_opts);
    const std::uint32_t w = o.width ? o.width : 72u;
    const std::uint32_t rows = o.height ? o.height : 28u;
    const scene::Scene sc = build_3d(*h, w, rows);

    if (o.format == TC_OUT_SVG || o.format == TC_OUT_PDF || o.format == TC_OUT_EPS) {
        std::string out;
        const tc_status st = render_scene_vector(sc, static_cast<int>(o.format), out, err);
        if (st != TC_OK) return st;
        return emit_bytes(out, buf, cap, out_len, err);
    }
    canvas::Canvas cv(w, rows);
    canvas::render_scene(sc, cv);
    std::string out;
    const tc_status st = render_cells_to_output(cv.to_buffer(), out_opts, out, err);
    if (st != TC_OK) return st;
    return emit_bytes(out, buf, cap, out_len, err);
}

// ── data 门面（内存文本 → 容错 DataFrame / 一行出图）───────────────────────

tc_data_handle tc_data_parse_csv(const char* text, const tc_data_csv_opts* opts,
                                 tc_error_handle* err) {
    if (text == nullptr) {
        set_err(err, "null csv text");
        return nullptr;
    }
    data::DataFrameOptions po;
    if (opts != nullptr) {
        po.delimiter = opts->delimiter == 0 ? 0 : static_cast<char>(opts->delimiter);
        po.has_header = opts->has_header != 0;
        po.comment = opts->comment == 0 ? 0 : static_cast<char>(opts->comment);
    }
    auto* h = new (std::nothrow) DataHandle();
    if (h == nullptr) {
        set_err(err, "out of memory");
        return nullptr;
    }
    h->frame = data::parse_dataframe(std::string_view(text), po);
    return reinterpret_cast<tc_data_handle>(h);
}

void tc_data_free(tc_data_handle data) { delete as_data(data); }

uint64_t tc_data_row_count(tc_data_handle data) {
    const DataHandle* h = as_data(data);
    return h == nullptr ? 0 : static_cast<uint64_t>(h->frame.row_count);
}

uint64_t tc_data_col_count(tc_data_handle data) {
    const DataHandle* h = as_data(data);
    return h == nullptr ? 0 : static_cast<uint64_t>(h->frame.columns.size());
}

tc_status tc_data_header(tc_data_handle data, uint64_t col, char* buf, uint64_t cap,
                         uint64_t* out_len, tc_error_handle* err) {
    const DataHandle* h = as_data(data);
    if (h == nullptr) return fail(err, "null data", TC_ERR_INVALID_HANDLE);
    if (out_len == nullptr) return fail(err, "null out_len", TC_ERR_INVALID_ARG);
    if (col >= h->frame.headers.size()) return fail(err, "column out of range", TC_ERR_INVALID_ARG);
    return emit_bytes(h->frame.headers[static_cast<std::size_t>(col)], buf, cap, out_len, err);
}

tc_status tc_data_cell_raw(tc_data_handle data, uint64_t row, uint64_t col, char* buf,
                           uint64_t cap, uint64_t* out_len, tc_error_handle* err) {
    const DataHandle* h = as_data(data);
    if (h == nullptr) return fail(err, "null data", TC_ERR_INVALID_HANDLE);
    if (out_len == nullptr) return fail(err, "null out_len", TC_ERR_INVALID_ARG);
    if (col >= h->frame.columns.size() || row >= h->frame.row_count) {
        return fail(err, "cell out of range", TC_ERR_INVALID_ARG);
    }
    const auto& cell = h->frame.columns[static_cast<std::size_t>(col)][static_cast<std::size_t>(row)];
    return emit_bytes(cell.raw, buf, cap, out_len, err);
}

tc_status tc_data_cell_number(tc_data_handle data, uint64_t row, uint64_t col, double* out,
                              tc_error_handle* err) {
    const DataHandle* h = as_data(data);
    if (h == nullptr) return fail(err, "null data", TC_ERR_INVALID_HANDLE);
    if (out == nullptr) return fail(err, "null out", TC_ERR_INVALID_ARG);
    if (col >= h->frame.columns.size() || row >= h->frame.row_count) {
        return fail(err, "cell out of range", TC_ERR_INVALID_ARG);
    }
    const auto& cell = h->frame.columns[static_cast<std::size_t>(col)][static_cast<std::size_t>(row)];
    if (!cell.is_numeric()) return fail(err, "cell is not numeric", TC_ERR_UNSUPPORTED);
    *out = cell.num;
    return TC_OK;
}

tc_status tc_data_cell_info(tc_data_handle data, uint64_t row, uint64_t col,
                            tc_cell_kind* kind, uint8_t* flags, tc_error_handle* err) {
    const DataHandle* h = as_data(data);
    if (h == nullptr) return fail(err, "null data", TC_ERR_INVALID_HANDLE);
    if (col >= h->frame.columns.size() || row >= h->frame.row_count) {
        return fail(err, "cell out of range", TC_ERR_INVALID_ARG);
    }
    const auto& cell = h->frame.columns[static_cast<std::size_t>(col)][static_cast<std::size_t>(row)];
    if (kind != nullptr) *kind = static_cast<tc_cell_kind>(cell.type);
    if (flags != nullptr) *flags = static_cast<std::uint8_t>(cell.flags);
    return TC_OK;
}

tc_status tc_auto_plot_text(const char* text, tc_error_handle* err) {
    if (text == nullptr || text[0] == '\0') return fail(err, "null/empty text", TC_ERR_INVALID_ARG);
    const std::string_view src(text);
    std::size_t first = 0;
    while (first < src.size() && (src[first] == ' ' || src[first] == '\t' ||
                                  src[first] == '\r' || src[first] == '\n')) {
        ++first;
    }
    const bool jsonl = first < src.size() && src[first] == '{';
    const auto picked = jsonl ? autoviz::auto_visualize_jsonl(src)
                              : autoviz::auto_visualize(data::parse_dataframe(src));
    if (!picked.has_value()) {
        return fail(err, std::string("auto_plot_text failed (") + describe(picked.error()) + ")",
                    status_to_tc(picked.error()));
    }
    const std::unique_ptr<chart::Chart> chart = picked->build_chosen();
    if (!chart) return fail(err, "cannot build chart", TC_ERR_UNSUPPORTED);
    const Status s = api::print_chart(*chart);
    if (!is_ok(s)) {
        return fail(err, std::string("auto_plot_text print failed (") + describe(s) + ")",
                    status_to_tc(s));
    }
    return TC_OK;
}

}  // extern "C"

