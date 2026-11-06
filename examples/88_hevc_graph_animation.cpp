// 88_hevc_graph_animation.cpp —— 自写 H.265(HEVC)「图的动画」（自包含、零渲染器依赖）
//
// 与 86_hevc_graph_animation（走 SceneRenderer 渲染整帧）不同，本示例只依赖 header-only
// 的 Bitmap 与 hevc_encode.cpp，用最少的像素绘制原语（点 / 线 / 圆盘 / 圆环 / 矩形）直接
// 画出一张「中心 hub + 环形节点」的节点-连线图，并让它动起来，再用 encode_hevc_annexb 编码成
// HEVC(.265) 裸流。这样它能在「只编 hevc_encode.cpp + Bitmap」的最小静态编译下直接跑通，
// 不依赖完整的 SceneRenderer / colormap 那一套。
//
// 规格（与需求一致）：
//   * 60 fps、约 5.0 s（300 帧）；
//   * 片头 0.5 s 静止（琥珀色边框 = 准备），片尾 0.5 s 静止（绿色边框 = 完成）；
//   * 中间 4 s 播放：整体缓慢旋转 + 节点呼吸式缩放 + 边上光点流动 + 底部进度条。
// 输出 .265 可被 ffprobe / VLC / PotPlayer 直接解码；
// 封装成 mp4 用：`ffmpeg -i out.265 -c copy out.mp4`（零重编码）。
//
// 跑法：88_hevc_graph_animation [out.265]
//   * 带参数：把动画编码成 .265 写到该路径；
//   * 不带参数：仅校验编码非空（ctest 用，不落盘）。
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

#include "termchart/image/bitmap.hpp"
#include "termchart/image/encode.hpp"
#include "termchart/image/hevc_encode.hpp"

namespace {

using termchart::image::Bitmap;
using termchart::image::EncodeOptions;
using termchart::image::encode_hevc_annexb;

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

// CTB=64 的对齐尺寸，避免编码器补黑边、码流更紧凑。
constexpr std::uint32_t kW = 256;
constexpr std::uint32_t kH = 128;
constexpr int kNodes = 12;

struct Rgb { std::uint8_t r, g, b; };

inline double frac(double x) { return x - std::floor(x); }

inline void set_px(Bitmap& bm, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    if (x < 0 || y < 0 || x >= static_cast<int>(kW) || y >= static_cast<int>(kH)) return;
    bm.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 0, r);
    bm.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 1, g);
    bm.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 2, b);
}

// 实心圆盘：以 line 宽描边一个半径 r/2 的圆弧，描边带正好填满半径 r 的圆盘。
void draw_disc(Bitmap& bm, double cx, double cy, double rad,
               std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    const int R = static_cast<int>(std::ceil(rad));
    for (int dy = -R; dy <= R; ++dy)
        for (int dx = -R; dx <= R; ++dx)
            if (dx * dx + dy * dy <= rad * rad)
                set_px(bm, static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, r, g, b);
}

void draw_ring(Bitmap& bm, double cx, double cy, double rad, double w,
               std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    const int R = static_cast<int>(std::ceil(rad + w));
    const double inner = rad - w / 2.0, outer = rad + w / 2.0;
    for (int dy = -R; dy <= R; ++dy)
        for (int dx = -R; dx <= R; ++dx) {
            const double d = std::sqrt(static_cast<double>(dx * dx + dy * dy));
            if (d >= inner && d <= outer)
                set_px(bm, static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, r, g, b);
        }
}

void draw_line(Bitmap& bm, double x0, double y0, double x1, double y1,
               std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    int ix0 = static_cast<int>(std::lround(x0)), iy0 = static_cast<int>(std::lround(y0));
    int ix1 = static_cast<int>(std::lround(x1)), iy1 = static_cast<int>(std::lround(y1));
    const int dx = std::abs(ix1 - ix0), dy = -std::abs(iy1 - iy0);
    const int sx = ix0 < ix1 ? 1 : -1, sy = iy0 < iy1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        set_px(bm, ix0, iy0, r, g, b);
        if (ix0 == ix1 && iy0 == iy1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; ix0 += sx; }
        if (e2 <= dx) { err += dx; iy0 += sy; }
    }
}

void draw_rect(Bitmap& bm, int x, int y, int w, int h,
               std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    for (int j = y; j < y + h; ++j)
        for (int i = x; i < x + w; ++i)
            set_px(bm, i, j, r, g, b);
}

// 近似 turbo 色环：t∈[0,1] → 蓝→青→绿→黄→红。
Rgb turbo(double t) {
    t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
    const auto c = [](double v) -> std::uint8_t {
        return static_cast<std::uint8_t>(255.0 * std::clamp(v, 0.0, 1.0));
    };
    return {c(1.5 - std::abs(4.0 * t - 3.0)),
            c(1.5 - std::abs(4.0 * t - 2.0)),
            c(1.5 - std::abs(4.0 * t - 1.0))};
}

double node_angle(int i, double rot) { return rot + i * (kTwoPi / kNodes); }

// 构造一帧。u∈[0,1] 动画进度（片头/片尾冻结在 0/1）；border 为边框状态色。
Bitmap build_frame(double u, Rgb border) {
    Bitmap bm(kW, kH, 3);
    draw_rect(bm, 0, 0, static_cast<int>(kW), static_cast<int>(kH), 14, 16, 28);  // 背景

    const double cx = kW / 2.0, cy = kH / 2.0;
    const double ringR = 46.0;
    const double rot = u * kTwoPi * 0.12;  // 整体缓慢旋转

    draw_ring(bm, cx, cy, ringR + 10.0, 1.0, 38, 44, 66);  // 外圈参考环

    // 边：spoke（hub→node）与 rim（node→next）
    const Rgb edge{70, 82, 120};
    for (int i = 0; i < kNodes; ++i) {
        const double a = node_angle(i, rot);
        const double px = cx + ringR * std::cos(a), py = cy + ringR * std::sin(a);
        draw_line(bm, cx, cy, px, py, edge.r, edge.g, edge.b);
        const int nxt = (i + 1) % kNodes;
        const double a2 = node_angle(nxt, rot);
        const double qx = cx + ringR * std::cos(a2), qy = cy + ringR * std::sin(a2);
        draw_line(bm, px, py, qx, qy, edge.r, edge.g, edge.b);
    }

    // 节点：turbo 配色 + 呼吸式缩放 + 外圈光环
    for (int i = 0; i < kNodes; ++i) {
        const double a = node_angle(i, rot);
        const double px = cx + ringR * std::cos(a), py = cy + ringR * std::sin(a);
        const double nr = 4.0 + 2.0 * std::sin(u * kTwoPi * 3.0 + i * 0.6);
        const Rgb c = turbo(static_cast<double>(i) / kNodes);
        draw_disc(bm, px, py, nr, c.r, c.g, c.b);
        draw_ring(bm, px, py, nr + 2.0, 1.0, 200, 210, 230);
    }

    // 中心 hub
    const double hr = 7.0 + 2.0 * std::sin(u * kTwoPi * 2.0);
    draw_disc(bm, cx, cy, hr, 255, 205, 120);
    draw_ring(bm, cx, cy, hr + 3.0, 1.0, 255, 230, 170);

    // 光点：沿 spoke 由 hub 向外流动，沿 rim 顺时针流动
    const double loopsSpoke = 2.0, loopsRim = 3.0;
    for (int i = 0; i < kNodes; ++i) {
        const double a = node_angle(i, rot);
        const double px = cx + ringR * std::cos(a), py = cy + ringR * std::sin(a);
        const double fs = frac(u * loopsSpoke + i * 0.1373);
        draw_disc(bm, cx + (px - cx) * fs, cy + (py - cy) * fs, 2.5, 235, 242, 255);
        const int nxt = (i + 1) % kNodes;
        const double a2 = node_angle(nxt, rot);
        const double qx = cx + ringR * std::cos(a2), qy = cy + ringR * std::sin(a2);
        const double fr = frac(u * loopsRim + i * 0.071);
        draw_disc(bm, px + (qx - px) * fr, py + (qy - py) * fr, 2.2, 235, 242, 255);
    }

    // 底部进度条
    const int bx = 8, by = static_cast<int>(kH) - 10, bw = static_cast<int>(kW) - 16, bh = 4;
    draw_rect(bm, bx, by, bw, bh, 36, 42, 60);
    const int fw = static_cast<int>(bw * u);
    if (fw > 1) draw_rect(bm, bx + 1, by + 1, fw - 2, bh - 2, 90, 200, 150);

    // 边框（状态色）：片头琥珀 / 片尾绿 / 播放蓝
    draw_rect(bm, 0, 0, static_cast<int>(kW), 2, border.r, border.g, border.b);
    draw_rect(bm, 0, static_cast<int>(kH) - 2, static_cast<int>(kW), 2, border.r, border.g, border.b);
    draw_rect(bm, 0, 0, 2, static_cast<int>(kH), border.r, border.g, border.b);
    draw_rect(bm, static_cast<int>(kW) - 2, 0, 2, static_cast<int>(kH), border.r, border.g, border.b);

    return bm;
}

}  // namespace

int main(int argc, char** argv) {
    constexpr int kFps = 60;
    constexpr int kTotal = 300;            // 5.0 s @ 60 fps
    constexpr int kLeadIn = 30;            // 片头静止 0.5 s（准备）
    constexpr int kLeadOut = 30;           // 片尾静止 0.5 s（完成）
    constexpr int kBody = kTotal - kLeadIn - kLeadOut;  // 240

    const Rgb kReady{255, 190, 80};   // 琥珀：准备
    const Rgb kPlay{80, 150, 255};    // 蓝：播放
    const Rgb kDone{120, 220, 160};   // 绿：完成

    std::vector<Bitmap> frames;
    frames.reserve(static_cast<std::size_t>(kTotal));

    const Bitmap intro = build_frame(0.0, kReady);   // 片头：静止"准备"
    for (int i = 0; i < kLeadIn; ++i) frames.push_back(intro);

    for (int i = 0; i < kBody; ++i) {                // 片体：播放
        const double u = kBody > 1 ? static_cast<double>(i) / (kBody - 1) : 0.0;
        frames.push_back(build_frame(u, kPlay));
    }

    const Bitmap outro = build_frame(1.0, kDone);    // 片尾：静止"完成"
    for (int i = 0; i < kLeadOut; ++i) frames.push_back(outro);

    // I_PCM 档裸流编码：opts 仅占位（裸流不关心 fps / 容器 codec）。
    const EncodeOptions opts{};
    const std::string annexb = encode_hevc_annexb(frames, opts);
    if (annexb.empty()) {
        std::fprintf(stderr, "encode_hevc_annexb produced no bytes\n");
        return 1;
    }

    int nals = 0;                                    // 期望 = 3 头 + kTotal 帧
    for (std::size_t i = 0; i + 3 < annexb.size(); ++i) {
        if (static_cast<std::uint8_t>(annexb[i]) == 0 &&
            static_cast<std::uint8_t>(annexb[i + 1]) == 0 &&
            static_cast<std::uint8_t>(annexb[i + 2]) == 0 &&
            static_cast<std::uint8_t>(annexb[i + 3]) == 1) {
            ++nals;
        }
    }

    std::printf("HEVC graph animation: %zu bytes, %d NALs, %d frames (%ux%u), %d fps, 5.0s\n",
                annexb.size(), nals, kTotal, kW, kH, kFps);

    if (argc > 1) {
        std::FILE* f = std::fopen(argv[1], "wb");
        if (!f) {
            std::fprintf(stderr, "cannot open %s\n", argv[1]);
            return 1;
        }
        std::fwrite(annexb.data(), 1, annexb.size(), f);
        std::fclose(f);
        std::printf("wrote %s\n", argv[1]);
    }
    return 0;
}
