// hevc_hwaccel_probe.cpp —— 硬件解码验收探针
//
// 由 tools/verify_hevc_hwaccel.py 自动编译，不属于库目标，也不进 ctest。
// argv: <qp> <W> <H> <形态:flat|hstep|checker|grad> <输出.265>
// 产出码流，并写 <输出.265>.yuv（原始 Y 平面，供 PSNR 计算）。
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/hevc_encode.hpp"

using namespace termchart::image;

static std::uint8_t cl(int v) { return (std::uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); }

int main(int argc, char** argv) {
    const int qp = argc > 1 ? std::atoi(argv[1]) : 26;
    const int W = argc > 2 ? std::atoi(argv[2]) : 128;
    const int H = argc > 3 ? std::atoi(argv[3]) : 64;
    const char* mode = argc > 4 ? argv[4] : "grad";
    const char* out265 = argc > 5 ? argv[5] : "tc_probe.265";
    if (W % 64 || H % 64 || W <= 0 || H <= 0) {
        std::fprintf(stderr, "size must be a positive multiple of 64\n");
        return 2;
    }

    Bitmap bmp(static_cast<std::uint32_t>(W), static_cast<std::uint32_t>(H), 4);
    std::vector<std::uint8_t> y(static_cast<std::size_t>(W) * H);
    for (int yy = 0; yy < H; ++yy)
        for (int xx = 0; xx < W; ++xx) {
            int r, g, b;
            if (!std::strcmp(mode, "flat")) {
                r = g = b = 131;
            } else if (!std::strcmp(mode, "checker")) {
                r = g = b = (((xx >> 1) ^ (yy >> 1)) & 1) ? 136 : 120;
            } else if (!std::strcmp(mode, "hstep")) {
                r = 30 + (xx / 8) * 14; g = 128; b = 128;
            } else {
                const int grad = (xx * 200) / (W - 1) + 20;
                const int tex = (((xx / 8) + (yy / 8)) & 1) ? 14 : -14;
                r = grad + tex; g = grad / 2 + tex / 2 + 40;
                b = (yy * 120) / (H - 1) + grad / 3;
            }
            const int R = cl(r), G = cl(g), B = cl(b);
            bmp.set(static_cast<std::uint32_t>(xx), static_cast<std::uint32_t>(yy), 0,
                    static_cast<std::uint8_t>(R));
            bmp.set(static_cast<std::uint32_t>(xx), static_cast<std::uint32_t>(yy), 1,
                    static_cast<std::uint8_t>(G));
            bmp.set(static_cast<std::uint32_t>(xx), static_cast<std::uint32_t>(yy), 2,
                    static_cast<std::uint8_t>(B));
            y[static_cast<std::size_t>(yy) * W + xx] =
                cl(((66 * R + 129 * G + 25 * B + 128) >> 8) + 16);
        }
    const std::string yuv = std::string(out265) + ".yuv";
    std::FILE* fp = std::fopen(yuv.c_str(), "wb");
    if (!fp) return 3;
    std::fwrite(y.data(), 1, y.size(), fp);
    std::fclose(fp);

    const std::vector<Bitmap> frames = {bmp};
    EncodeOptions opts;
    opts.video_mode = VideoEncodeMode::Intra;
    opts.video_qp = qp;
    const auto bs = encode_hevc_annexb(std::span<const Bitmap>{frames}, opts);
    if (bs.empty()) {
        std::fprintf(stderr, "encode failed qp=%d %dx%d %s\n", qp, W, H, mode);
        return 1;
    }
    fp = std::fopen(out265, "wb");
    if (!fp) return 3;
    std::fwrite(bs.data(), 1, bs.size(), fp);
    std::fclose(fp);
    std::printf("%zu\n", bs.size());
    return 0;
}
