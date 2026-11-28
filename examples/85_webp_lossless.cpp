// 85_webp_lossless.cpp —— 自包含 VP8L lossless WebP 编码器端到端演示
//
// 与 83_encode_jpeg 同一条真链路（不经 canvas/term，直接
// Scene → gui::SceneRenderer → image::Bitmap → encode_webp）：构图用
// termchart::scene::SceneBuilder 真实 API，渲染用 gui::SceneRenderer 超采样位图，
// 最后交给本项目自带的零依赖 WebP 编码器落盘。验证"WebP 编码导出"缺口闭合。
//
// 为什么选这组图形：连续渐变 + 硬边文字是**无损**编码的价值所在——换成 JPEG 会
// 在渐变里出现色带、在文字边缘出现振铃；VP8L 逐像素还原，编码前后完全一致。
//
// 跑法：85_webp_lossless [out.webp]
//   * 带参数：把图编码成 WebP 写到该路径；
//   * 不带参数：仅校验编码非空 + 容器魔数（ctest 用，不落盘）。
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/gui/scene_renderer.hpp"
#include "termchart/media/image/webp_encode.hpp"
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
using termchart::image::encode_webp;
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
constexpr int kBands = 24;

// 逐带线性变化的色相：JPEG 在这个尺度上会出现可见色带，VP8L 不会。
Rgb band_color(int i) {
    const double t = static_cast<double>(i) / static_cast<double>(kBands - 1);
    const auto mix = [t](int a, int b) {
        return static_cast<int>(a + (b - a) * t + 0.5);
    };
    return Rgb{static_cast<std::uint8_t>(mix(31, 253)),
               static_cast<std::uint8_t>(mix(119, 141)),
               static_cast<std::uint8_t>(mix(180, 60))};
}

Style muted_style() {
    Style s;
    s.set_fg(Color::rgb(Rgb{90, 90, 90}));
    return s;
}

Scene build_scene() {
    Scene scene;
    SceneBuilder b(scene);

    // 横向渐变带：每带一个实心矩形，带间不留缝，色相逐带推进。
    const double band_h = (kPlotBottom - kPlotTop) / static_cast<double>(kBands);
    for (int i = 0; i < kBands; ++i) {
        const double y = kPlotTop + band_h * static_cast<double>(i);
        // 带宽随高度收窄，形成三角形渐变区（仍逐带实心，便于肉眼检查色带）。
        const double inset = 220.0 * static_cast<double>(i) / static_cast<double>(kBands);
        b.fill(Color::rgb(band_color(i)));
        b.rect(Rect{kPlotLeft + inset, y, (kPlotRight - kPlotLeft) - 2.0 * inset,
                    band_h + 0.5});
    }

    // 外框 + 参考线：硬边几何，编码失真会立刻在边框上显现。
    // 注意：RectOp 在 fill 状态非空时**优先实心填充**（StatefulVisitor 的既有语义），
    // 不清空 fill 就会把整块绘图区刷成最后一带的颜色。
    b.fill(Color::none());
    b.stroke(Color::rgb(Rgb{60, 60, 60}), 1.0);
    b.rect(Rect{kPlotLeft, kPlotTop, kPlotRight - kPlotLeft, kPlotBottom - kPlotTop});
    b.stroke(Color::rgb(Rgb{240, 240, 240}), 1.0);
    b.move_to(kPlotLeft, (kPlotTop + kPlotBottom) / 2.0);
    b.line_to(kPlotRight, (kPlotTop + kPlotBottom) / 2.0);

    b.text(Point{kPlotLeft, 24.0},
           "Lossless gradient bands (WebP VP8L demo)", muted_style());
    return scene;
}

// 只做容器级自检：RIFF....WEBP + VP8L 子块。像素级正确性由单元测试里那份
// 独立 VP8L 解码器守住（不依赖本编码器自己的码字）。
bool has_vp8l_container(const std::string& bytes) {
    static const std::string riff = "RIFF";
    static const std::string webp = "WEBP";
    static const std::string vp8l = "VP8L";
    return bytes.size() > 20 && bytes.compare(0, 4, riff) == 0 &&
           bytes.compare(8, 4, webp) == 0 && bytes.compare(12, 4, vp8l) == 0;
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

    std::vector<termchart::image::Bitmap> frames;
    frames.push_back(bitmap);
    const std::string webp = encode_webp(frames);
    if (webp.empty()) {
        std::fprintf(stderr, "encode_webp produced no bytes\n");
        return 1;
    }
    if (!has_vp8l_container(webp)) {
        std::fprintf(stderr, "encoded bytes are not a RIFF/WEBP/VP8L container\n");
        return 1;
    }
    std::printf("encoded WebP (VP8L lossless): %zu bytes (bitmap %ux%u, %u ch)\n",
                webp.size(), bitmap.width(), bitmap.height(), bitmap.channels());

    if (argc > 1) {
        std::ofstream out(argv[1], std::ios::binary);
        if (!out) {
            std::fprintf(stderr, "cannot open %s\n", argv[1]);
            return 1;
        }
        out.write(webp.data(), static_cast<std::streamsize>(webp.size()));
        std::printf("wrote %s\n", argv[1]);
    }
    return 0;
}
