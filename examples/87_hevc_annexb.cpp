// 87_hevc_annexb.cpp —— 自写 HEVC (H.265) 裸流（Annex-B）编码器直接用法演示
//
// 与 84 / 86（走 MP4 容器封装，内部再调 HEVC 编码器）不同，本示例直接调用最底层接口
//   termchart::image::encode_hevc_annexb()
// 把一串 Bitmap 编码成「原始 Annex-B 字节流」——VPS + SPS + PPS + 每帧一个 IDR slice，
// 即标准 .265 文件。这对"需要把码流喂给裸流解析器 / 自定义封装 / 学习 HEVC 语法结构"的
// 场景最有用：输出不带 MP4 盒子，ffprobe 也只需 `ffprobe -i xxx.265`。
//
// 当前实现与 h264_encode 同哲学的 **I_PCM 档**：每个 Coding Tree Unit 递归二分到 8x8 叶，
// 叶直接以 PCM 模式输出原始采样（无损、无变换/量化/熵编码），用最少的码流语法把整条
// YUV→NAL→裸流链路打通，且能被标准解码器逐字节精确还原（FFmpeg 9.0.2 已全帧校验）。
//
// 跑法：87_hevc_annexb [out.265]
//   * 带参数：把动画编码成 .265 写到该路径；
//   * 不带参数：仅校验编码非空（ctest 用，不落盘）。
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "termchart/image/bitmap.hpp"
#include "termchart/image/encode.hpp"
#include "termchart/image/hevc_encode.hpp"

namespace {

using termchart::image::Bitmap;
using termchart::image::EncodeOptions;
using termchart::image::encode_hevc_annexb;

// 一帧：水平渐变（红→蓝）+ 一条随时间左右移动的高亮竖条，制造时域变化。
// 用 3 通道 RGB；编码器内部转 YUV420，再按 8x8 PCM 块逐块写出。
Bitmap make_frame(std::uint32_t w, std::uint32_t h, int phase, int frames) {
    Bitmap bm(w, h, 3);
    const int bar_x =
        static_cast<int>((static_cast<double>(phase) / frames) * (w - 4.0)) + 2;
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const std::uint8_t r = static_cast<std::uint8_t>(x * 255 / w);
            const std::uint8_t b = static_cast<std::uint8_t>(255 - x * 255 / w);
            std::uint8_t g = static_cast<std::uint8_t>(y * 255 / h);
            if (x >= static_cast<std::uint32_t>(bar_x) &&
                x < static_cast<std::uint32_t>(bar_x) + 4) {
                g = 255;  // 高亮条
            }
            bm.set(x, y, 0, r);
            bm.set(x, y, 1, g);
            bm.set(x, y, 2, b);
        }
    }
    return bm;
}

}  // namespace

int main(int argc, char** argv) {
    // 取 64 的整数倍（HEVC CTB=64 的对齐尺寸），避免编码器补黑边、码流更紧凑。
    constexpr std::uint32_t kW = 128;
    constexpr std::uint32_t kH = 64;
    constexpr int kFrames = 8;

    std::vector<Bitmap> frames;
    frames.reserve(static_cast<std::size_t>(kFrames));
    for (int i = 0; i < kFrames; ++i)
        frames.push_back(make_frame(kW, kH, i, kFrames));

    // I_PCM 档裸流编码：opts 仅占位（裸流不关心 fps / 容器 codec）。
    const EncodeOptions opts{};
    const std::string annexb = encode_hevc_annexb(frames, opts);
    if (annexb.empty()) {
        std::fprintf(stderr, "encode_hevc_annexb produced no bytes\n");
        return 1;
    }

    // 统计 NAL 单元数（Annex-B 起始码 00 00 00 01 出现次数）：
    // 期望 VPS + SPS + PPS + 每帧 1 个 slice = 3 + kFrames。
    int nals = 0;
    for (std::size_t i = 0; i + 3 < annexb.size(); ++i) {
        if (static_cast<std::uint8_t>(annexb[i]) == 0 &&
            static_cast<std::uint8_t>(annexb[i + 1]) == 0 &&
            static_cast<std::uint8_t>(annexb[i + 2]) == 0 &&
            static_cast<std::uint8_t>(annexb[i + 3]) == 1) {
            ++nals;
        }
    }

    std::printf("HEVC Annex-B: %zu bytes, %d NAL units, %d frames (%ux%u)\n",
                annexb.size(), nals, kFrames, kW, kH);

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
