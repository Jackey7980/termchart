// 83_encode_jpeg.cpp —— 自包含 baseline JPEG 编码器端到端演示
//
// 走与 55_gui_scene_png 完全相同的真链路（不经 canvas/term，直接
// Scene → gui::SceneRenderer → image::Bitmap → encode_jpeg）：构图用
// termchart::scene::SceneBuilder 真实 API，渲染用 gui::SceneRenderer 超采样位图，
// 最后用本项目自带的零依赖 JPEG 编码器落盘。验证"JPEG 编码导出"缺口闭合。
//
// 跑法：83_encode_jpeg [out.jpg]
//   * 带参数：把图编码成 JPEG 写到该路径；
//   * 不带参数：仅校验编码非空（ctest 用，不落盘）。
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/gui/scene_renderer.hpp"
#include "termchart/image/jpeg_encode.hpp"
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
using termchart::image::encode_jpeg;
using termchart::scene::Scene;
using termchart::scene::SceneBuilder;

constexpr std::uint32_t kCanvasWidth = 800;
constexpr std::uint32_t kCanvasHeight = 450;
constexpr int kSupersample = 3;
constexpr double kTextCellW = 9.0;
constexpr double kTextCellH = 16.0;

constexpr double kPlotLeft = 70.0;
constexpr double kPlotTop = 60.0;
constexpr double kPlotRight = 760.0;
constexpr double kPlotBottom = 380.0;
constexpr int kPoints = 12;
constexpr double kValueMax = 100.0;
constexpr Rgb kGridColor = {210, 210, 210};
constexpr Rgb kFrameColor = {90, 90, 90};
constexpr Rgb kRevenueColor = {31, 119, 180};
constexpr Rgb kCostColor = {214, 39, 40};

double revenue_at(int i) { return 55.0 + 18.0 * std::sin(i * 0.55) + i * 1.2; }
double cost_at(int i)    { return 35.0 + 12.0 * std::sin(i * 0.45 + 1.0) + i * 0.8; }

void polyline(SceneBuilder& b, const std::vector<Point>& pts) {
    b.move_to(pts.front());
    for (std::size_t i = 1; i < pts.size(); ++i) b.line_to(pts[i]);
}

Style muted_style() {
    Style s;
    s.set_fg(Color::rgb(kFrameColor));
    return s;
}

Scene build_scene() {
    Scene scene;
    SceneBuilder b(scene);
    const auto to_y = [](double v) {
        return kPlotBottom - (kPlotBottom - kPlotTop) * v / kValueMax;
    };
    const auto x_at = [](int i) {
        const double t = static_cast<double>(i) / static_cast<double>(kPoints - 1);
        return kPlotLeft + (kPlotRight - kPlotLeft) * t;
    };

    // 横向网格
    b.stroke(Color::rgb(kGridColor), 1.0);
    for (int g = 0; g <= 5; ++g) {
        const double v = kValueMax * g / 5.0;
        const double y = to_y(v);
        b.move_to(kPlotLeft, y);
        b.line_to(kPlotRight, y);
        const std::string label = std::to_string(static_cast<int>(v));
        b.text(Point{kPlotLeft - 8.0 - static_cast<double>(label.size()) * kTextCellW,
                     y - kTextCellH / 2.0},
               label, muted_style());
    }
    // 外框
    b.stroke(Color::rgb(kFrameColor), 1.0);
    b.rect(Rect{kPlotLeft, kPlotTop, kPlotRight - kPlotLeft, kPlotBottom - kPlotTop});

    // 两条系列
    std::vector<Point> revenue, cost;
    revenue.reserve(kPoints);
    cost.reserve(kPoints);
    for (int i = 0; i < kPoints; ++i) {
        revenue.push_back(Point{x_at(i), to_y(revenue_at(i))});
        cost.push_back(Point{x_at(i), to_y(cost_at(i))});
    }
    b.stroke(Color::rgb(kRevenueColor), 2.5);
    polyline(b, revenue);
    b.stroke(Color::rgb(kCostColor), 2.0);
    polyline(b, cost);

    b.text(Point{kPlotLeft, 24.0},
           "Quarterly Revenue vs Cost (JPEG export demo)", muted_style());
    return scene;
}

}  // namespace

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
    const std::string jpg = encode_jpeg(bitmap, 85);
    if (jpg.empty()) {
        std::fprintf(stderr, "encode_jpeg produced no bytes\n");
        return 1;
    }
    std::printf("encoded JPEG: %zu bytes (bitmap %ux%u, %u ch)\n", jpg.size(),
                bitmap.width(), bitmap.height(), bitmap.channels());

    if (argc > 1) {
        std::ofstream out(argv[1], std::ios::binary);
        if (!out) {
            std::fprintf(stderr, "cannot open %s\n", argv[1]);
            return 1;
        }
        out.write(jpg.data(), static_cast<std::streamsize>(jpg.size()));
        std::printf("wrote %s\n", argv[1]);
    }
    return 0;
}
