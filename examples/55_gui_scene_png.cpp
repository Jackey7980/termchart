// 55_gui_scene_png.cpp —— 生产级观感折线图：deterministic Scene → 超采样位图 → PNG
//
// 这是 examples/55 的可运行产物，验证 gui/scene_renderer 的完整链路
// （与终端栈平行：不经 canvas/term，直接 Scene → image::Bitmap → encode_png）：
//   * 几何：连续折线路径、整圆 marker（arc{0, 2π}）、描边矩形外框、
//     实心色块图例、8px 实/6px 空虚线（Scene 无虚线语义，用 MoveTo/LineTo
//     交替拼出）；
//   * 文本：标题/灰色副标题/y 轴刻度/x 轴月份/图例名/注脚——全部 ASCII
//     （点阵字体暂只内置 ASCII，中文与全角符号会画占位框，故标题的
//     破折号用 ASCII 连字符代替 em dash）；
//   * 质量：数据由确定性公式生成（禁止随机，同一构建永远同一张图），
//     autofit=false 让 Scene 坐标即像素坐标（布局常量可直接对齐像素），
//     supersample=3 盒式降采样给出抗锯齿，PNG 走 stored-deflate 编码。
// 跑法：55_gui_scene_png [out.png]；不带参数只校验编码、不落盘（ctest 用）。
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
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
using termchart::gui::RenderOptions;
using termchart::gui::SceneRenderer;
using termchart::image::encode_png;
using termchart::scene::Scene;
using termchart::scene::SceneBuilder;

// ── 画布与文本格（背景/前景用 RenderOptions 默认值，不在此重复）──
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

constexpr int kPointCount = 12;        // 每个系列的数据点数（12 个月）
constexpr double kValueMax = 100.0;    // y 轴值域上限（下限恒为 0）
constexpr int kGridLineCount = 6;      // 横向网格线数（含 0 与 kValueMax）

// ── 线宽与 marker ──
constexpr double kFrameWidth = 1.0;
constexpr double kRevenueWidth = 2.5;
constexpr double kCostWidth = 2.0;
constexpr double kMarginWidth = 1.5;
constexpr double kMarkerRadius = 3.0;
constexpr double kMarkerWidth = 1.5;
constexpr double kTwoPi = 6.283185307179586;

// ── 虚线节奏（Scene 无虚线语义：MoveTo/LineTo 交替拼 8px 实 + 6px 空）──
constexpr double kDashLen = 8.0;
constexpr double kDashGap = 6.0;

// ── 图例（绘图区内右上角，纵向排）──
constexpr double kLegendSwatch = 12.0;      // 色块边长
constexpr double kLegendSwatchTextGap = 6.0;
constexpr double kLegendRowStep = 20.0;
constexpr double kLegendRightPad = 16.0;    // 距绘图区右缘
constexpr double kLegendTopPad = 16.0;      // 距绘图区上缘

// ── 标题/轴/注脚布局 ──
constexpr double kTitleY = 24.0;
constexpr double kSubtitleY = 46.0;
constexpr double kXLabelGap = 10.0;   // x 标签距绘图区下缘
constexpr double kAxisTextPad = 8.0;  // y 刻度文本与绘图区左缘的间距
constexpr double kFooterY = 630.0;

// ── 颜色（网格比外框暗一档；灰字用于辅助信息）──
constexpr Rgb kFrameColor{120, 120, 120};
constexpr Rgb kGridColor{56, 56, 56};
constexpr Rgb kMutedText{148, 148, 148};
constexpr Rgb kRevenueColor{86, 156, 214};
constexpr Rgb kCostColor{234, 150, 86};
constexpr Rgb kMarginColor{126, 196, 126};

// 确定性数据公式：只依赖 i，无随机数。
double revenue_at(int i) {
    return 55.0 + 18.0 * std::sin(i * 0.55) + i * 1.2;
}
double cost_at(int i) {
    return 35.0 + 12.0 * std::sin(i * 0.45 + 1.0) + i * 0.8;
}

// 连续折线：move_to 起点，逐段 line_to。
void polyline(SceneBuilder& b, const std::vector<Point>& points) {
    b.move_to(points.front());
    for (std::size_t i = 1; i < points.size(); ++i) {
        b.line_to(points[i]);
    }
}

// 虚线折线：沿每段以 8px 实 / 6px 空 交替发 MoveTo/LineTo 小段
// （相位逐段重置，段首永远是实线段）。
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

// 灰色小字（副标题/轴刻度/注脚共用）。
Style muted_style() {
    Style style;
    style.set_fg(Color::rgb(kMutedText));
    return style;
}

Scene build_scene() {
    Scene scene;
    SceneBuilder b(scene);

    // y 坐标映射：值 0..100 → 绘图区 bottom..top 线性。
    const auto to_y = [](double value) {
        return kPlotBottom - (kPlotBottom - kPlotTop) * value / kValueMax;
    };
    // 第 i 个数据点的 x：绘图区内均分。
    const auto x_at = [](int i) {
        const double t = static_cast<double>(i) /
                         static_cast<double>(kPointCount - 1);
        return kPlotLeft + (kPlotRight - kPlotLeft) * t;
    };

    // 1) 横向网格（先画，外框随后压住两端）+ 左侧 y 轴刻度。
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

    // 3) x 轴月份标签：与数据点同样均分，水平居中于各自 x。
    constexpr std::string_view kMonths[kPointCount] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < kPointCount; ++i) {
        const double w =
            static_cast<double>(kMonths[i].size()) * kTextCellW;
        b.text(Point{x_at(i) - w / 2.0, kPlotBottom + kXLabelGap}, kMonths[i],
               axis_style);
    }

    // 4) 三条系列（同一份确定性公式，margin = revenue - cost）。
    std::vector<Point> revenue;
    std::vector<Point> cost;
    std::vector<Point> margin;
    revenue.reserve(kPointCount);
    cost.reserve(kPointCount);
    margin.reserve(kPointCount);
    for (int i = 0; i < kPointCount; ++i) {
        const double rev = revenue_at(i);
        const double cst = cost_at(i);
        revenue.push_back(Point{x_at(i), to_y(rev)});
        cost.push_back(Point{x_at(i), to_y(cst)});
        margin.push_back(Point{x_at(i), to_y(rev - cst)});
    }

    b.stroke(Color::rgb(kRevenueColor), kRevenueWidth);
    polyline(b, revenue);
    b.stroke(Color::rgb(kRevenueColor), kMarkerWidth);
    for (const Point& p : revenue) {
        b.arc(p, kMarkerRadius, 0.0, kTwoPi);  // 整圆 marker
    }

    b.stroke(Color::rgb(kCostColor), kCostWidth);
    polyline(b, cost);

    b.stroke(Color::rgb(kMarginColor), kMarginWidth);
    dashed_polyline(b, margin);

    // 5) 图例：实心色块 + 系列名，纵向排，贴绘图区右上角。
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

    // 6) 标题（左上两行）与底部注脚。
    b.text(Point{kPlotLeft, kTitleY},
           "Quarterly Revenue vs Cost - termchart GUI renderer");
    b.text(Point{kPlotLeft, kSubtitleY},
           "1200x675 canvas, autofit off (scene units = pixels), supersample 3x",
           muted_style());
    b.text(Point{kPlotLeft, kFooterY},
           "deterministic scene -> supersampled bitmap -> PNG (stored deflate)",
           muted_style());

    return scene;
}

}  // namespace

// argv[1] 存在则把 PNG 字节写入该路径；否则只校验编码非空、打印统计，
// 不落盘（ctest 里跑示例不产生文件）。返回 0/1，与仓库其他示例一致。
int main(int argc, char** argv) {
    const Scene scene = build_scene();

    RenderOptions options;
    options.width = kCanvasWidth;
    options.height = kCanvasHeight;
    options.autofit = false;
    options.supersample = kSupersample;
    options.text_cell_w = kTextCellW;
    options.text_cell_h = kTextCellH;

    SceneRenderer renderer(options);
    const termchart::image::Bitmap bitmap = renderer.render(scene);
    const std::string png = encode_png(bitmap);
    if (png.empty()) {
        std::fprintf(stderr, "encode_png produced no bytes\n");
        return 1;
    }

    if (argc > 1) {
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
        std::printf("wrote %s (%zu bytes)\n", argv[1], png.size());
    } else {
        std::printf("encoded %zu bytes (no output path given)\n", png.size());
    }
    return 0;
}
