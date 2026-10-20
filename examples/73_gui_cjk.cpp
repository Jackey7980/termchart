// 73_gui_cjk.cpp —— GUI 文本出口接系统 TTF 后的中文上屏验证
//
// 验证 gui/font 的混合字体后端：ASCII 仍走内置 5×7 点阵，CJK 等非 ASCII
// 码点改走 image::TtfFont 解析的系统字体（Windows 自动试
// C:/Windows/Fonts/msyh.ttc / simhei.ttf；环境变量 TERMCHART_GUI_FONT 可
// 显式指定）。找不到字体或解析失败时 fail-safe 退回点阵占位框——不崩、
// 不留白。整条链路与终端栈平行，不经 canvas/term：
//   Scene（标题/X 轴标签/图例全中文）→ SceneRenderer → image::Bitmap → PNG
//
// 等宽语义不变：X 轴标签居中用 gui::measure_text 量宽（CJK 按显示宽度占
// 2 列），advance 恒为整格，TTF 只决定格内像素。
// 跑法：73_gui_cjk [out.png]；不带参数只校验编码、不落盘（ctest 用）。
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
#include "termchart/gui/font.hpp"
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
using termchart::gui::measure_text;
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
constexpr double kPlotTop = 100.0;
constexpr double kPlotRight = 1140.0;
constexpr double kPlotBottom = 560.0;

constexpr int kPointCount = 3;        // 三个月
constexpr double kValueMax = 100.0;   // y 轴值域上限（下限恒为 0）
constexpr int kGridLineCount = 5;     // 横向网格线数（含 0 与 kValueMax）

// ── 线宽与 marker ──
constexpr double kFrameWidth = 1.0;
constexpr double kRevenueWidth = 2.5;
constexpr double kCostWidth = 2.0;
constexpr double kMarkerRadius = 4.0;
constexpr double kMarkerWidth = 1.5;
constexpr double kTwoPi = 6.283185307179586;

// ── 图例布局 ──
constexpr double kLegendSwatch = 12.0;
constexpr double kLegendSwatchTextGap = 6.0;
constexpr double kLegendRowStep = 22.0;
constexpr double kLegendRightPad = 16.0;
constexpr double kLegendTopPad = 16.0;

// ── 标题/轴/注脚布局 ──
constexpr double kTitleY = 28.0;
constexpr double kSubtitleY = 56.0;
constexpr double kXLabelGap = 12.0;   // x 标签距绘图区下缘
constexpr double kAxisTextPad = 8.0;  // y 刻度文本与绘图区左缘的间距
constexpr double kFooterY = 630.0;

// ── 颜色 ──
constexpr Rgb kFrameColor{120, 120, 120};
constexpr Rgb kGridColor{56, 56, 56};
constexpr Rgb kMutedText{148, 148, 148};
constexpr Rgb kTitleColor{235, 235, 235};
constexpr Rgb kRevenueColor{86, 156, 214};
constexpr Rgb kCostColor{234, 150, 86};

// 确定性数据（禁随机：同一构建永远同一张图）。
constexpr double kRevenue[kPointCount] = {46.0, 61.0, 74.0};
constexpr double kCost[kPointCount] = {32.0, 41.0, 53.0};

// 连续折线：move_to 起点，逐段 line_to。
void polyline(SceneBuilder& b, const std::vector<Point>& points) {
    b.move_to(points.front());
    for (std::size_t i = 1; i < points.size(); ++i) {
        b.line_to(points[i]);
    }
}

Style muted_style() {
    Style style;
    style.set_fg(Color::rgb(kMutedText));
    return style;
}

// 标题/轴刻度等按显示宽度居中或右对齐的文本，量宽统一走 gui::measure_text
// （ASCII 1 列、CJK 2 列），与 draw_text 的推进同口径。
double text_width(std::string_view text) {
    return measure_text(text, kTextCellW, kTextCellH).width;
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

    // 1) 横向网格 + 左侧 y 轴刻度（数字 ASCII，走点阵路径）。
    const Style axis_style = muted_style();
    b.stroke(Color::rgb(kGridColor), 1.0);
    for (int g = 0; g < kGridLineCount; ++g) {
        const double value = kValueMax * static_cast<double>(g) /
                             static_cast<double>(kGridLineCount - 1);
        const double y = to_y(value);
        b.move_to(kPlotLeft, y);
        b.line_to(kPlotRight, y);
        const std::string label = std::to_string(static_cast<int>(value));
        const double label_w = text_width(label);
        b.text(Point{kPlotLeft - kAxisTextPad - label_w, y - kTextCellH / 2.0},
               label, axis_style);
    }

    // 2) 绘图区外框。
    b.stroke(Color::rgb(kFrameColor), kFrameWidth);
    b.rect(Rect{kPlotLeft, kPlotTop, kPlotRight - kPlotLeft,
                kPlotBottom - kPlotTop});

    // 3) X 轴中文标签：一月 / 二月 / 三月，按显示宽度居中于各自数据点。
    constexpr std::string_view kMonths[kPointCount] = {"一月", "二月", "三月"};
    for (int i = 0; i < kPointCount; ++i) {
        const double w = text_width(kMonths[i]);
        b.text(Point{x_at(i) - w / 2.0, kPlotBottom + kXLabelGap}, kMonths[i],
               axis_style);
    }

    // 4) 两个系列（确定性常量数据）。
    std::vector<Point> revenue;
    std::vector<Point> cost;
    revenue.reserve(kPointCount);
    cost.reserve(kPointCount);
    for (int i = 0; i < kPointCount; ++i) {
        revenue.push_back(Point{x_at(i), to_y(kRevenue[i])});
        cost.push_back(Point{x_at(i), to_y(kCost[i])});
    }

    b.stroke(Color::rgb(kRevenueColor), kRevenueWidth);
    polyline(b, revenue);
    b.stroke(Color::rgb(kRevenueColor), kMarkerWidth);
    for (const Point& p : revenue) {
        b.arc(p, kMarkerRadius, 0.0, kTwoPi);  // 整圆 marker
    }

    b.stroke(Color::rgb(kCostColor), kCostWidth);
    polyline(b, cost);
    b.stroke(Color::rgb(kCostColor), kMarkerWidth);
    for (const Point& p : cost) {
        b.arc(p, kMarkerRadius, 0.0, kTwoPi);
    }

    // 5) 图例：实心色块 + 中文系列名，纵向排，贴绘图区右上角。
    struct LegendEntry {
        std::string_view name;
        Rgb color;
    };
    constexpr LegendEntry kLegend[] = {
        {"收入", kRevenueColor},
        {"成本", kCostColor}};
    double max_name_w = 0.0;
    for (const LegendEntry& entry : kLegend) {
        max_name_w = std::max(max_name_w, text_width(entry.name));
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

    // 6) 标题（中文，白）+ 副标题 + 底部注脚。
    Style title_style;
    title_style.set_fg(Color::rgb(kTitleColor));
    b.text(Point{kPlotLeft, kTitleY}, "中文测试", title_style);
    b.text(Point{kPlotLeft, kSubtitleY},
           "gui::draw_text 已接系统 TTF；找不到字体则降级点阵占位框",
           muted_style());
    b.text(Point{kPlotLeft, kFooterY},
           "确定性数据 -> Scene -> 超采样位图 -> PNG（ASCII 走点阵，CJK 走 TTF）",
           muted_style());

    return scene;
}

// 仅用于开跑前打印一行提示：探测与 font.cpp 相同的系统字体候选是否在场。
// 渲染路径本身不经过这里——找到与否都由库内部 fail-safe 处理。
void print_font_hint() {
#if defined(_WIN32)
    const char* candidates[] = {"C:/Windows/Fonts/msyh.ttc",
                                "C:/Windows/Fonts/simhei.ttf"};
#elif defined(__APPLE__)
    const char* candidates[] = {"/System/Library/Fonts/PingFang.ttc"};
#else
    const char* candidates[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc"};
#endif
    for (const char* path : candidates) {
        std::ifstream f(path, std::ios::binary);
        if (f.is_open()) {
            std::printf("system CJK font: %s (found)\n", path);
            return;
        }
    }
    std::printf("system CJK font: none of the candidates found "
                "-> CJK falls back to 5x7 placeholder boxes\n");
}

// 统计标题带内相对背景色的差异像素：TTF 真字形 vs 占位框的数量级差异
// （诊断输出，让"中文到底上没上屏"不靠肉眼猜）。背景默认 {20,20,20}，
// 判据用通道差绝对值之和 > kInkThreshold，抗背景非零。
std::size_t count_band_ink(const termchart::image::Bitmap& bitmap, double y0, double y1,
                           Rgb background) {
    constexpr int kInkThreshold = 30;
    const auto uy0 = static_cast<std::uint32_t>(std::max(0.0, std::floor(y0)));
    const auto uy1 = static_cast<std::uint32_t>(
        std::min(static_cast<double>(bitmap.height()), std::ceil(y1)));
    const auto diff = [background](std::uint8_t v, std::uint8_t b) noexcept {
        return static_cast<int>(v) - static_cast<int>(b) < 0
                   ? static_cast<int>(b) - static_cast<int>(v)
                   : static_cast<int>(v) - static_cast<int>(b);
    };
    std::size_t ink = 0;
    for (std::uint32_t y = uy0; y < uy1; ++y) {
        for (std::uint32_t x = 0; x < bitmap.width(); ++x) {
            if (diff(bitmap.at(x, y, 0), background.r) + diff(bitmap.at(x, y, 1), background.g) +
                    diff(bitmap.at(x, y, 2), background.b) >
                kInkThreshold) {
                ++ink;
            }
        }
    }
    return ink;
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

    print_font_hint();
    // 标题带 ink：TTF 中文字形（四字共 8 列 × 16px 行的真 ink）应在数百像素
    // 量级；若掉成占位框则只有约两条虚线的量级（~150 px）。
    std::printf("title band ink: %zu px (x-axis CJK labels drawn below)\n",
                count_band_ink(bitmap, kTitleY - 4.0, kTitleY + kTextCellH + 4.0,
                               options.background));

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
