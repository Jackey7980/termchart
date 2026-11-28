// 86_hevc_graph_animation.cpp —— 用自写 H.265(HEVC) 编码器导出一段"图的动画"
//
// 内容：一张带中心 hub 的环形节点-连线图。边上光点流动（spoke 由 hub 向外、rim 顺时针）、
// 节点呼吸式缩放、整体缓慢旋转。片头 0.5s 静止（准备），片尾 0.5s 静止（反应），中间 4s 播放。
// 60 fps、约 5.0s、HEVC(Main profile, I_PCM 档) 封装进 MP4，可被标准播放器 / ffprobe 解码。
//
// 跑法：86_hevc_graph_animation [out.mp4]
//   * 带参数：编码成 HEVC MP4 写到该路径；
//   * 不带参数：仅校验导出非空（ctest 用，不落盘）。
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

#include "termchart/api/export_image.hpp"
#include "termchart/color/colormap.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/gui/scene_renderer.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/encode.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/scene/scene.hpp"

namespace {

using termchart::Color;
using termchart::Point;
using termchart::Rgb;
using termchart::Style;
using termchart::image::Bitmap;
using termchart::image::EncodeOptions;
using termchart::image::ImageFormat;
using termchart::image::VideoCodec;
using termchart::scene::Scene;
using termchart::scene::SceneBuilder;
using termchart::gui::RenderOptions;
using termchart::gui::SceneRenderer;

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

// 取 64 的整数倍（HEVC CTB=64 的对齐尺寸），避免编码端补黑边。
constexpr std::uint32_t kW = 512;
constexpr std::uint32_t kH = 320;
constexpr int kFps = 60;
constexpr int kTotalFrames = 300;            // 5.0s @ 60fps
constexpr int kLeadInFrames = 30;           // 片头静止 0.5s（准备）
constexpr int kLeadOutFrames = 30;          // 片尾静止 0.5s（反应）
constexpr int kBodyFrames = kTotalFrames - kLeadInFrames - kLeadOutFrames;  // 240

constexpr int kNodes = 18;
constexpr double kCx = kW / 2.0;
constexpr double kCy = 160.0;
constexpr double kRingR = 110.0;
constexpr double kHubR = 10.0;

constexpr Rgb kBg{14, 16, 28};
constexpr Rgb kEdge{70, 82, 120};
constexpr Rgb kFrame{40, 46, 70};
constexpr Rgb kTitle{210, 220, 235};
constexpr Rgb kSub{150, 160, 180};
constexpr Rgb kPulse{235, 242, 255};
constexpr Rgb kHub{255, 205, 120};
constexpr Rgb kStatus{120, 220, 160};
constexpr Rgb kBarBg{36, 42, 60};
constexpr Rgb kBarFg{90, 200, 150};
constexpr Rgb kRingRef{38, 44, 66};
constexpr Rgb kNodeRing{200, 210, 230};
constexpr Rgb kHubRing{255, 230, 170};

inline double frac(double x) { return x - std::floor(x); }

Point node_pos(int i, double rot) {
    const double ang = rot + static_cast<double>(i) * (kTwoPi / kNodes);
    return Point{kCx + kRingR * std::cos(ang), kCy + kRingR * std::sin(ang)};
}

// 实心圆盘：画一段半径为 r/2 的圆弧并以线宽 r 描边，描边带正好填满半径 r 的圆盘。
void fill_disc(SceneBuilder& b, Point c, double r, Color col) {
    if (r <= 0.0) return;
    b.arc(c, r / 2.0, 0.0, kTwoPi);
    b.stroke(col, r);
}

void stroke_ring(SceneBuilder& b, Point c, double r, double w, Color col) {
    b.arc(c, r, 0.0, kTwoPi);
    b.stroke(col, w);
}

void stroke_segment(SceneBuilder& b, Point a, Point bb, Color col, double w) {
    b.move_to(a.x, a.y);
    b.line_to(bb.x, bb.y);
    b.stroke(col, w);
}

void stroke_rect(SceneBuilder& b, double x, double y, double w, double h, Color col, double lw) {
    b.move_to(x, y);
    b.line_to(x + w, y);
    b.line_to(x + w, y + h);
    b.line_to(x, y + h);
    b.line_to(x, y);
    b.stroke(col, lw);
}

// 构造某一帧的 Scene。u ∈ [0,1] 是"动画进度"（片头/片尾冻结在 0/1）；
// progress ∈ [0,1] 是整段视频时间进度（进度条用）；status 是底部状态词。
Scene build_frame(double u, double progress, const char* status) {
    Scene scene;
    SceneBuilder b(scene);

    const double rot = u * kTwoPi * 0.12;  // 整体缓慢旋转

    // 外框
    stroke_rect(b, 8.0, 8.0, kW - 16.0, kH - 16.0, Color::rgb(kFrame), 1.0);

    // 外圈参考环
    stroke_ring(b, Point{kCx, kCy}, kRingR + 16.0, 1.0, Color::rgb(kRingRef));

    // 边：spoke（hub→node）与 rim（node→next）
    for (int i = 0; i < kNodes; ++i) {
        const Point p = node_pos(i, rot);
        const int nxt = (i + 1) % kNodes;
        const Point q = node_pos(nxt, rot);
        stroke_segment(b, Point{kCx, kCy}, p, Color::rgb(kEdge), 1.1);
        stroke_segment(b, p, q, Color::rgb(kEdge), 1.1);
    }

    // 节点：turbo 配色 + 呼吸式缩放 + 外圈光环
    for (int i = 0; i < kNodes; ++i) {
        const Point p = node_pos(i, rot);
        const double nr = 5.0 + 2.2 * std::sin(u * kTwoPi * 3.0 + i * 0.6);
        const Rgb c = termchart::color::sample(termchart::color::ColormapKind::Turbo,
                                               static_cast<double>(i) / kNodes);
        fill_disc(b, p, nr, Color::rgb(c));
        stroke_ring(b, p, nr + 2.5, 1.0, Color::rgb(kNodeRing));
    }

    // 中心 hub
    const double hr = kHubR + 2.0 * std::sin(u * kTwoPi * 2.0);
    fill_disc(b, Point{kCx, kCy}, hr, Color::rgb(kHub));
    stroke_ring(b, Point{kCx, kCy}, hr + 3.0, 1.0, Color::rgb(kHubRing));

    // 光点：沿 spoke 由 hub 向 node 流动，沿 rim 顺时针流动
    const double loops_spoke = 2.0;
    const double loops_rim = 3.0;
    for (int i = 0; i < kNodes; ++i) {
        const Point p = node_pos(i, rot);
        const double fs = frac(u * loops_spoke + i * 0.1373);
        const Point s1 = Point{kCx + (p.x - kCx) * fs, kCy + (p.y - kCy) * fs};
        fill_disc(b, s1, 3.0, Color::rgb(kPulse));

        const int nxt = (i + 1) % kNodes;
        const Point q = node_pos(nxt, rot);
        const double fr = frac(u * loops_rim + i * 0.071);
        const Point s2 = Point{p.x + (q.x - p.x) * fr, p.y + (q.y - p.y) * fr};
        fill_disc(b, s2, 2.6, Color::rgb(kPulse));
    }

    // 标题与副标题（GUI 位图字体仅含 ASCII，故用纯 ASCII 文案）
    b.text(Point{16.0, 18.0}, "termchart H.265 HEVC graph animation",
           Style{}.set_fg(Color::rgb(kTitle)));
    b.text(Point{16.0, 38.0}, "60 fps | 5.0 s | H.265 / HEVC",
           Style{}.set_fg(Color::rgb(kSub)));

    // 底部状态与时间
    b.text(Point{kCx - 22.0, kH - 40.0}, status, Style{}.set_fg(Color::rgb(kStatus)));
    char timebuf[32];
    std::snprintf(timebuf, sizeof(timebuf), "%.1f s / 5.0 s", progress * 5.0);
    b.text(Point{kW - 96.0, kH - 40.0}, timebuf, Style{}.set_fg(Color::rgb(kSub)));

    // 进度条（先描边外框，再填充进度）
    const double bx = 16.0, by = kH - 24.0, bw = kW - 32.0, bh = 8.0;
    b.rect(termchart::Rect{bx, by, bw, bh});
    b.stroke(Color::rgb(kBarBg), 1.0);
    const double fw = bw * progress;
    if (fw > 1.0) {
        b.rect(termchart::Rect{bx + 1.0, by + 1.0, fw - 2.0, bh - 2.0});
        b.fill(Color::rgb(kBarFg));
    }

    return scene;
}

}  // namespace

int main(int argc, char** argv) {
    RenderOptions options;
    options.width = kW;
    options.height = kH;
    options.autofit = false;
    options.scale = 1.0;
    options.supersample = 2;
    options.background = kBg;
    options.foreground = kTitle;
    options.text_cell_w = 9.0;
    options.text_cell_h = 16.0;
    SceneRenderer renderer(options);

    std::vector<Bitmap> frames;
    frames.reserve(static_cast<std::size_t>(kTotalFrames));

    const EncodeOptions eo{.fps = kFps, .video_codec = VideoCodec::H265};

    auto render_u = [&](double u, double progress, const char* status) -> Bitmap {
        const Scene scene = build_frame(u, progress, status);
        return renderer.render(scene);
    };

    // 片头：静止"准备"
    const Bitmap intro = render_u(0.0, 0.0, "READY");
    for (int i = 0; i < kLeadInFrames; ++i) frames.push_back(intro);

    // 片体：播放
    for (int i = 0; i < kBodyFrames; ++i) {
        const double u = kBodyFrames > 1 ? static_cast<double>(i) / (kBodyFrames - 1) : 0.0;
        const double progress = static_cast<double>(kLeadInFrames + i) / (kTotalFrames - 1);
        frames.push_back(render_u(u, progress, "PLAYING"));
    }

    // 片尾：静止"完成"
    const Bitmap outro = render_u(1.0, 1.0, "DONE");
    for (int i = 0; i < kLeadOutFrames; ++i) frames.push_back(outro);

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

    std::printf("HEVC MP4: %zu bytes, %d frames (%ux%u), codec H265, %d fps\n",
                bytes.size(), kTotalFrames, kW, kH, kFps);
    return 0;
}
