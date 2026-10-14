// 58_gui_hover.cpp —— GUI 悬停出数据：命中 → 浮层图元 → Scene 重放（无头可跑）
//
// 演示"把鼠标放在数据点上、图上出现该点读数"在 GUI 平行栈里怎么走：
//   图表渲染时登记 chart::HitRegion（形状 + 不透明 payload）
//     → 鼠标位置做 interaction::topmost_hit（纯几何，禁图表名）
//       → 命中的 payload 查表得到文本
//         → 浮层翻译成 RectOp/TextOp，叠加进 Scene 副本
//           → gui::SceneRenderer 重放整帧（渲染器零改动，它不知道交互存在）
//
// 与终端侧 examples/21_hover_tooltip 的对照：上游完全同一套 HitRegion /
// topmost_hit，差别只在"最后一厘米"——终端用 cell 补丁浮层（覆盖即破坏，
// 要逐格还原），GUI 每帧重建 Scene，浮层只是多塞几个图元，因此无需 patch。
//
// 几何与观感沿用 examples/55（同一张三系列折线图），额外为 36 个数据点登记
// 半径 10px 的命中圆（后登记者在上，故登记顺序 margin → cost → revenue，
// 让 revenue 的 marker 优先响应）。数据全确定性，无随机。
//
// 跑法：
//   58_gui_hover                只编码底图与悬停帧、校验二者不同，不落盘（ctest 用）
//   58_gui_hover out.png        落盘底图
//   58_gui_hover out.png X Y    落盘"鼠标在 (X,Y)"的悬停帧
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "termchart/chart/hit_region.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/gui/hover_chart.hpp"
#include "termchart/gui/scene_renderer.hpp"
#include "termchart/image/png_encode.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/scene/scene.hpp"

namespace {

using termchart::Color;
using termchart::Point;
using termchart::Rect;
using termchart::Rgb;
using termchart::Style;
using termchart::chart::HitRegion;
using termchart::gui::HoverLayer;
using termchart::gui::RenderOptions;
using termchart::image::Bitmap;
using termchart::image::encode_png;
using termchart::scene::Scene;
using termchart::scene::SceneBuilder;

// ── 画布与文本格（与 55 一致；背景/前景取 RenderOptions 默认值）──
constexpr std::uint32_t kCanvasWidth = 1200;
constexpr std::uint32_t kCanvasHeight = 675;
constexpr int kSupersample = 3;
constexpr double kTextCellW = 9.0;
constexpr double kTextCellH = 16.0;

// ── 绘图区（autofit=false：以下即像素坐标）──
constexpr double kPlotLeft = 90.0;
constexpr double kPlotTop = 70.0;
constexpr double kPlotRight = 1140.0;
constexpr double kPlotBottom = 560.0;

constexpr int kPointCount = 12;
constexpr double kValueMax = 100.0;
constexpr int kGridLineCount = 6;

// ── 线宽 / marker / 命中半径 ──
constexpr double kFrameWidth = 1.0;
constexpr double kRevenueWidth = 2.5;
constexpr double kCostWidth = 2.0;
constexpr double kMarginWidth = 1.5;
constexpr double kMarkerRadius = 3.0;
constexpr double kMarkerWidth = 1.5;
constexpr double kTwoPi = 6.283185307179586;
constexpr double kHitRadius = 10.0;  // 命中圆比 marker 大，便于悬停（Scene 单位）

// ── 虚线节奏（Scene 无虚线语义：MoveTo/LineTo 交替拼 8px 实 + 6px 空）──
constexpr double kDashLen = 8.0;
constexpr double kDashGap = 6.0;

// ── 图例布局（绘图区内右上角，纵向排）──
constexpr double kLegendSwatch = 12.0;
constexpr double kLegendSwatchTextGap = 6.0;
constexpr double kLegendRowStep = 20.0;
constexpr double kLegendRightPad = 16.0;
constexpr double kLegendTopPad = 16.0;

// ── 标题/轴/注脚布局 ──
constexpr double kTitleY = 24.0;
constexpr double kSubtitleY = 46.0;
constexpr double kXLabelGap = 10.0;
constexpr double kAxisTextPad = 8.0;
constexpr double kFooterY = 630.0;

// ── 颜色 ──
constexpr Rgb kFrameColor{120, 120, 120};
constexpr Rgb kGridColor{56, 56, 56};
constexpr Rgb kMutedText{148, 148, 148};
constexpr Rgb kRevenueColor{86, 156, 214};
constexpr Rgb kCostColor{234, 150, 86};
constexpr Rgb kMarginColor{126, 196, 126};

double revenue_at(int i) {
    return 55.0 + 18.0 * std::sin(i * 0.55) + i * 1.2;
}

double cost_at(int i) {
    return 35.0 + 12.0 * std::sin(i * 0.45 + 1.0) + i * 0.8;
}

void polyline(SceneBuilder& b, const std::vector<Point>& points) {
    b.move_to(points.front());
    for (std::size_t i = 1; i < points.size(); ++i) {
        b.line_to(points[i]);
    }
}

void dashed_polyline(SceneBuilder& b, const std::vector<Point>& points) {
    for (std::size_t i = 1; i < points.size(); ++i) {
        const Point a = points[i - 1];
        const Point c = points[i];
        const double dx = c.x - a.x;
        const double dy = c.y - a.y;
        const double len = std::sqrt(dx * dx + dy * dy);
        const double ux = dx / len;
        const double uy = dy / len;
        for (double s = 0.0; s < len; s += kDashLen + kDashGap) {
            const double e = std::min(len, s + kDashLen);
            b.move_to(a.x + ux * s, a.y + uy * s);
            b.line_to(a.x + ux * e, a.y + uy * e);
        }
    }
}

Style muted_style() {
    Style style;
    style.set_fg(Color::rgb(kMutedText));
    return style;
}

// 图：底图 Scene + 命中区域 + payload 文本 + 一个"必然命中"的默认悬停点。
struct Chart {
    Scene scene;
    std::vector<HitRegion> regions;
    std::vector<std::string> labels;  // labels[payload-1]
    Point key_point{};                // revenue 第 7 个点，供无参模式自检
};

std::string point_label(std::string_view series, std::string_view month, double value) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%.1f", value);
    std::string out(series);
    out += '\n';
    out += month;
    out += ": ";
    out += buf;
    return out;
}

void add_hit(Chart& chart, Point at, std::string label) {
    const std::uint32_t payload =
        static_cast<std::uint32_t>(chart.labels.size()) + 1U;
    chart.regions.push_back(HitRegion::circle(at, kHitRadius, payload));
    chart.labels.push_back(std::move(label));
}

Chart build_chart() {
    Chart chart;
    SceneBuilder b(chart.scene);

    const auto to_y = [](double value) {
        return kPlotBottom - (kPlotBottom - kPlotTop) * value / kValueMax;
    };
    const auto x_at = [](int i) {
        const double t = static_cast<double>(i) /
                         static_cast<double>(kPointCount - 1);
        return kPlotLeft + (kPlotRight - kPlotLeft) * t;
    };

    // 1) 横向网格 + 左侧 y 轴刻度。
    const Style axis_style = muted_style();
    b.stroke(Color::rgb(kGridColor), 1.0);
    for (int g = 0; g < kGridLineCount; ++g) {
        const double value = kValueMax * static_cast<double>(g) /
                             static_cast<double>(kGridLineCount - 1);
        const double y = to_y(value);
        b.move_to(kPlotLeft, y);
        b.line_to(kPlotRight, y);
        const std::string label = std::to_string(static_cast<int>(value));
        const double label_w = static_cast<double>(label.size()) * kTextCellW;
        b.text(Point{kPlotLeft - kAxisTextPad - label_w, y - kTextCellH / 2.0},
               label, axis_style);
    }

    // 2) 绘图区外框。
    b.stroke(Color::rgb(kFrameColor), kFrameWidth);
    b.rect(Rect{kPlotLeft, kPlotTop, kPlotRight - kPlotLeft,
                kPlotBottom - kPlotTop});

    // 3) x 轴月份标签。
    constexpr std::string_view kMonths[kPointCount] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < kPointCount; ++i) {
        const double w = static_cast<double>(kMonths[i].size()) * kTextCellW;
        b.text(Point{x_at(i) - w / 2.0, kPlotBottom + kXLabelGap}, kMonths[i],
               axis_style);
    }

    // 4) 三条系列（确定性公式，margin = revenue - cost）。
    std::vector<Point> revenue;
    std::vector<Point> cost;
    std::vector<Point> margin;
    std::vector<double> rev_v;
    std::vector<double> cst_v;
    std::vector<double> mar_v;
    for (int i = 0; i < kPointCount; ++i) {
        const double rev = revenue_at(i);
        const double cst = cost_at(i);
        const double mar = rev - cst;
        revenue.push_back(Point{x_at(i), to_y(rev)});
        cost.push_back(Point{x_at(i), to_y(cst)});
        margin.push_back(Point{x_at(i), to_y(mar)});
        rev_v.push_back(rev);
        cst_v.push_back(cst);
        mar_v.push_back(mar);
    }

    b.stroke(Color::rgb(kRevenueColor), kRevenueWidth);
    polyline(b, revenue);
    b.stroke(Color::rgb(kRevenueColor), kMarkerWidth);
    for (const Point& p : revenue) {
        b.arc(p, kMarkerRadius, 0.0, kTwoPi);
    }

    b.stroke(Color::rgb(kCostColor), kCostWidth);
    polyline(b, cost);

    b.stroke(Color::rgb(kMarginColor), kMarginWidth);
    dashed_polyline(b, margin);

    // 5) 登记命中区域：后登记者在上 → margin→cost→revenue，让 revenue marker
    //    在重叠处优先响应（topmost_hit 取最后命中者）。
    for (int i = 0; i < kPointCount; ++i) {
        add_hit(chart, margin[i], point_label("Margin", kMonths[i], mar_v[i]));
    }
    for (int i = 0; i < kPointCount; ++i) {
        add_hit(chart, cost[i], point_label("Cost", kMonths[i], cst_v[i]));
    }
    for (int i = 0; i < kPointCount; ++i) {
        add_hit(chart, revenue[i], point_label("Revenue", kMonths[i], rev_v[i]));
    }
    chart.key_point = revenue[6];

    // 6) 图例：实心色块 + 系列名。
    struct LegendEntry {
        std::string_view name;
        Rgb color;
    };
    constexpr LegendEntry kLegend[] = {
        {"Revenue", kRevenueColor},
        {"Cost", kCostColor},
        {"Margin", kMarginColor}};
    double max_name_w = 0.0;
    for (const LegendEntry& entry : kLegend) {
        max_name_w = std::max(max_name_w,
                              static_cast<double>(entry.name.size()) * kTextCellW);
    }
    const double legend_x = kPlotRight - kLegendRightPad - kLegendSwatch -
                            kLegendSwatchTextGap - max_name_w;
    double row_y = kPlotTop + kLegendTopPad;
    for (const LegendEntry& entry : kLegend) {
        b.fill(Color::rgb(entry.color));
        b.rect(Rect{legend_x, row_y, kLegendSwatch, kLegendSwatch});
        b.text(Point{legend_x + kLegendSwatch + kLegendSwatchTextGap,
                     row_y + (kLegendSwatch - kTextCellH) / 2.0},
               entry.name);
        row_y += kLegendRowStep;
    }

    // 7) 标题/副标题/注脚。
    b.text(Point{kPlotLeft, kTitleY},
           "Hover to inspect data - termchart GUI hover layer");
    b.text(Point{kPlotLeft, kSubtitleY},
           "HitRegion + topmost_hit (shared with terminal) -> tooltip primitives -> Scene replay",
           muted_style());
    b.text(Point{kPlotLeft, kFooterY},
           "deterministic scene -> hit test at mouse -> appended RectOp/TextOp -> supersampled PNG",
           muted_style());

    return chart;
}

}  // namespace

int main(int argc, char** argv) {
    const Chart chart = build_chart();

    RenderOptions options;
    options.width = kCanvasWidth;
    options.height = kCanvasHeight;
    options.autofit = false;
    options.supersample = kSupersample;
    options.text_cell_w = kTextCellW;
    options.text_cell_h = kTextCellH;

    const HoverLayer layer(chart.scene, chart.regions, chart.labels, options);

    if (argc <= 1) {
        // ctest 模式：底图与"鼠标落在 revenue 第 7 点上"的悬停帧都要能编码，
        // 且两者必须不同——否则说明浮层根本没叠进 Scene。
        const Bitmap base = layer.render_base();
        const Bitmap hovered = layer.render_at(chart.key_point.x, chart.key_point.y);
        const std::string base_png = encode_png(base);
        const std::string hover_png = encode_png(hovered);
        if (base_png.empty() || hover_png.empty()) {
            std::fprintf(stderr, "encode_png produced no bytes\n");
            return 1;
        }
        if (base_png == hover_png) {
            std::fprintf(stderr, "hover frame identical to base frame\n");
            return 1;
        }
        std::printf("base %zu bytes, hover %zu bytes (no output path given)\n",
                    base_png.size(), hover_png.size());
        return 0;
    }

    const bool want_hover = argc > 3;
    const Bitmap bitmap = want_hover
                              ? layer.render_at(std::atof(argv[2]), std::atof(argv[3]))
                              : layer.render_base();
    const std::string png = encode_png(bitmap);
    if (png.empty()) {
        std::fprintf(stderr, "encode_png produced no bytes\n");
        return 1;
    }

    std::ofstream out(argv[1], std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }
    out.write(png.data(), static_cast<std::streamsize>(png.size()));
    out.close();
    if (!out) {
        std::fprintf(stderr, "failed to write %s\n", argv[1]);
        return 1;
    }
    std::printf("wrote %s (%zu bytes, %s)\n", argv[1], png.size(),
                want_hover ? "hover frame" : "base frame");
    return 0;
}
