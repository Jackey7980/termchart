// 84_hevc_video.cpp —— 自写 HEVC (H.265) 视频导出端到端演示
//
// 走真实链路：Scene（折线图）→ gui::SceneRenderer → image::Bitmap（每帧相位偏移做动画），
// 再用 api::save_image 多帧重载把一串 Bitmap 封装成 HEVC MP4。验证"视频导出"缺口闭合，
// 且产出的 MP4 能被标准播放器 / PotPlayer 播放（见 mp4_mux 的 tkhd / edts 正确性）。
//
// 跑法：84_hevc_video [out.mp4]
//   * 带参数：把动画编码成 HEVC MP4 写到该路径；
//   * 不带参数：仅校验导出非空（ctest 用，不落盘）。
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

#include "termchart/api/export_image.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/gui/scene_renderer.hpp"
#include "termchart/image/bitmap.hpp"
#include "termchart/image/encode.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/scene/scene.hpp"

namespace {

using termchart::Color;
using termchart::Point;
using termchart::Rgb;
using termchart::Style;
using termchart::gui::RenderOptions;
using termchart::gui::SceneRenderer;
using termchart::image::Bitmap;
using termchart::image::EncodeOptions;
using termchart::image::ImageFormat;
using termchart::image::VideoCodec;
using termchart::scene::Scene;
using termchart::scene::SceneBuilder;

constexpr std::uint32_t kWidth = 480;
constexpr std::uint32_t kHeight = 270;
constexpr int kFrames = 12;
constexpr double kTextCellW = 9.0;
constexpr double kTextCellH = 16.0;

// 一条随 phase 滚动的正弦曲线，配坐标轴，构成一帧动画。
Scene build_scene(double phase) {
    Scene scene;
    SceneBuilder b(scene);
    const double left = 50.0, right = static_cast<double>(kWidth) - 30.0;
    const double top = 50.0, bottom = static_cast<double>(kHeight) - 40.0;

    b.stroke(Color::rgb({120, 120, 120}), 1.0);
    b.move_to(left, top);
    b.line_to(left, bottom);
    b.line_to(right, bottom);

    b.stroke(Color::rgb({31, 119, 180}), 2.5);
    b.move_to(left, (top + bottom) / 2.0);
    for (int x = 0; x <= 240; ++x) {
        const double t = static_cast<double>(x) / 240.0;
        const double px = left + (right - left) * t;
        const double v = 0.5 + 0.35 * std::sin(t * 6.2831853 * 2.0 + phase);
        const double py = bottom - (bottom - top) * v;
        b.line_to(px, py);
    }
    return scene;
}

}  // namespace

int main(int argc, char** argv) {
    RenderOptions options;
    options.width = kWidth;
    options.height = kHeight;
    options.autofit = false;
    options.supersample = 2;
    options.text_cell_w = kTextCellW;
    options.text_cell_h = kTextCellH;
    SceneRenderer renderer(options);

    std::vector<Bitmap> frames;
    frames.reserve(static_cast<std::size_t>(kFrames));
    for (int i = 0; i < kFrames; ++i) {
        const Scene scene = build_scene(static_cast<double>(i) * 0.4);
        frames.push_back(renderer.render(scene));
    }

    const EncodeOptions eo{.fps = 12, .video_codec = VideoCodec::H265};

    // ctest 路径：仅校验编码非空，不落盘。
    const std::string bytes = termchart::image::encode(frames, ImageFormat::Mp4, eo);
    if (bytes.empty()) {
        std::fprintf(stderr, "encode (HEVC MP4) produced no bytes\n");
        return 1;
    }

    if (argc > 1) {
        const auto st = termchart::api::save_image(frames, argv[1], ImageFormat::Mp4, eo);
        if (st != termchart::ErrorCode::Ok) {
            std::fprintf(stderr, "save_image failed: %d\n", static_cast<int>(st));
            return 1;
        }
        std::printf("wrote %s\n", argv[1]);
    }

    std::printf("HEVC MP4: %zu bytes, %d frames (%ux%u), codec H265\n", bytes.size(),
                kFrames, kWidth, kHeight);
    return 0;
}
