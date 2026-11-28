// 75_gui_cjk_4d.cpp —— 终极缝合怪：4D 超立方体 + TTF 中文 + GUI 像素画布
//
// 把此前的三块积木缝成一幅确定性的 PNG：
//   * math4d/chart4d（4D 管线）：16 顶点 / 32 边超立方体，绕 XW + YZ 两平面
//     旋转（固定帧角，无时钟依赖），Projector4D 两阶段 Vec4→Vec3→画布；
//   * chart4d::wire4d：发射边（按两端平均深度着色）与顶点方块的 scene::Scene；
//   * gui/font（FontBackend 组合链）：标题/副标题/注脚全中文，非 ASCII 码点走
//     image::TtfFont 系统字体（Windows 试 msyh.ttc/simhei.ttf），ASCII 仍走
//     内置 5×7 点阵；找不到字体 fail-safe 退占位框，不崩不留白；
//   * gui::SceneRenderer：3× 超采样抗锯齿，Scene → image::Bitmap → PNG。
// 与 74（终端 braille 差分重绘）的分工：74 是动画管道，75 是单帧出版质量；
// 真窗口交互版（拖拽旋转/滚轮缩放）住 plugins/gui_win32 hypercube_4d_demo。
// 跑法：75_gui_cjk_4d [out.png]；不带参数只校验编码、不落盘（ctest 用）。
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/chart3d/common.hpp"     // chart3d::make_camera
#include "termchart/chart3d/projector.hpp"  // chart3d::Projector3D
#include "termchart/chart4d/projector4d.hpp"
#include "termchart/chart4d/wire.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/gui/font.hpp"
#include "termchart/gui/scene_renderer.hpp"
#include "termchart/media/image/png_encode.hpp"
#include "termchart/math3d/vec3.hpp"
#include "termchart/math4d/camera4.hpp"
#include "termchart/math4d/mat5.hpp"
#include "termchart/math4d/vec4.hpp"
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

// ── 画布与文本格（像素坐标，autofit=false）──
constexpr std::uint32_t kCanvasWidth = 1200;
constexpr std::uint32_t kCanvasHeight = 675;
constexpr int kSupersample = 3;
constexpr double kTextCellW = 9.0;
constexpr double kTextCellH = 16.0;

// ── 固定帧角（确定性：同一构建永远同一张图）──
constexpr double kAngleXW = 1.15;  // XW 平面转角（弧度）
constexpr double kAngleYZ = 0.60;  // YZ 平面转角（弧度）

// ── 4D 线框参数（Scene 单位 = 像素）──
constexpr double kPointSize = 4.0;   // 顶点方块边长
constexpr double kEdgeWidth = 2.0;   // GUI 粗线（终端默认 1.0 不变）

// ── 文本布局 ──
constexpr double kTitleX = 90.0;
constexpr double kTitleY = 30.0;
constexpr double kSubtitleY = 58.0;
constexpr double kFooterY = 636.0;
constexpr double kFooterRightPad = 90.0;

// ── 颜色 ──
constexpr Rgb kTitleColor{235, 235, 235};
constexpr Rgb kMutedText{150, 150, 150};

// 16 个超立方体顶点：4 个坐标各取 ±1（与 74 同一构造）。
std::vector<termchart::math4d::Vec4> hypercube_vertices() {
    std::vector<termchart::math4d::Vec4> v;
    v.reserve(16);
    for (int i = 0; i < 16; ++i) {
        v.push_back(termchart::math4d::Vec4{(i & 1) ? 1.0 : -1.0, (i & 2) ? 1.0 : -1.0,
                                            (i & 4) ? 1.0 : -1.0, (i & 8) ? 1.0 : -1.0});
    }
    return v;
}

// 32 条边：仅连二进制位相差 1 的顶点对（4-正则，16×4/2=32）。
std::vector<termchart::chart4d::Edge> hypercube_edges() {
    std::vector<termchart::chart4d::Edge> e;
    for (int i = 0; i < 16; ++i) {
        for (int b = 0; b < 4; ++b) {
            const int j = i ^ (1 << b);
            if (j > i) {
                e.push_back({static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(j)});
            }
        }
    }
    return e;
}

double text_width(std::string_view text) {
    return measure_text(text, kTextCellW, kTextCellH).width;
}

Style text_style(Rgb color) {
    Style style;
    style.set_fg(Color::rgb(color));
    return style;
}

// wire4d 产几何 Scene；SceneBuilder 只持引用不清空，可直接追中文文本
// （SceneRenderer 的文本路径按最终分辨率直绘并置顶，不会被几何盖住）。
Scene build_scene() {
    const auto verts = hypercube_vertices();
    const auto edges = hypercube_edges();

    // 阶段二：内层 3D 轨道相机（视角固定；宽高给像素，aspect 与画布一致）。
    const termchart::math3d::Camera cam = termchart::chart3d::make_camera(
        static_cast<double>(kCanvasWidth), static_cast<double>(kCanvasHeight),
        /*azimuth*/ 30.0, /*elevation*/ 20.0, /*fovy*/ 55.0,
        termchart::math3d::Vec3{0.0, 0.0, 0.0}, /*radius*/ 2.2);
    const auto inner = std::make_shared<termchart::chart3d::Projector3D>(cam);

    // 阶段一：4D 相机架在 +W 轴，60° 视场。
    termchart::math4d::Camera4 cam4;
    cam4.distance = 3.0;
    cam4.focal = termchart::math4d::focal_from_fov_deg(60.0);

    const termchart::math4d::Mat5 rot =
        termchart::math4d::rotate_xw(kAngleXW) * termchart::math4d::rotate_yz(kAngleYZ);
    const termchart::chart4d::Projector4D projector(rot, cam4, inner);

    termchart::chart4d::Wire4dOpts wo;
    wo.cols = static_cast<double>(kCanvasWidth);
    wo.rows = static_cast<double>(kCanvasHeight);
    wo.point_size = kPointSize;
    wo.edge_width = kEdgeWidth;
    wo.draw_vertices = true;

    Scene scene = termchart::chart4d::wire4d(verts, edges, wo, projector);

    SceneBuilder b(scene);
    b.text(Point{kTitleX, kTitleY}, "四维超立方体", text_style(kTitleColor));
    b.text(Point{kTitleX, kSubtitleY}, "XW + YZ 双平面自转 · 两阶段透视投影",
           text_style(kMutedText));
    // 注脚右对齐（CJK 按显示宽度占 2 列，量宽与推进同口径）。
    constexpr std::string_view kFooter =
        "16 顶点 / 32 边 · 边按深度着色 · 交互版：plugins/gui_win32 hypercube_4d_demo";
    b.text(Point{kCanvasWidth - kFooterRightPad - text_width(kFooter), kFooterY}, kFooter,
           text_style(kMutedText));
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

// 标题带内相对背景色的差异像素：TTF 真字形 vs 占位框的数量级差异
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
    // 标题带 ink：TTF 中文字形（六字共 12 列 × 16px 行的真 ink）应在数百像素
    // 量级；若掉成占位框则只有约两条虚线的量级（~150 px）。
    std::printf("title band ink: %zu px (XW=%.2f rad, YZ=%.2f rad)\n",
                count_band_ink(bitmap, kTitleY - 4.0, kTitleY + kTextCellH + 4.0,
                               options.background),
                kAngleXW, kAngleYZ);

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
