// av1_frame_probe.cpp —— 真实 Bitmap 像素 → 整帧 IVF + 编码器重建 YUV。
// 用法: av1_frame_probe <w> <h> <q> <rdo_mode> <rdo_partition> <out.ivf> <out.yuv> [flat] [prediction_only]
// 纹理由确定性公式生成（渐变 + 棋盘 + 方块），覆盖平坦/边缘/多模式区域。
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "av1_encode.hpp"

using namespace termchart::image;

namespace {

int clamp_u8(int v) { return std::clamp(v, 0, 255); }

Bitmap make_texture(int w, int h, bool flat) {
    Bitmap bitmap(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), 3);
    for (int y = 0; y < h; ++y) {
        if (flat) {
            for (int x = 0; x < w; ++x) {
                for (std::uint8_t c = 0; c < 3; ++c)
                    bitmap.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), c,
                               128);
            }
            continue;
        }
        for (int x = 0; x < w; ++x) {
            const int ramp = (x * 4 + y * 2) % 256;
            const int check = ((x / 12) + (y / 12)) % 2 ? 36 : -36;
            const int cx = w / 2, cy = h / 2;
            const int blob = std::max(0, 90 - (std::abs(x - cx) + std::abs(y - cy)));
            bitmap.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 0,
                       static_cast<std::uint8_t>(clamp_u8(ramp + check + blob)));
            bitmap.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 1,
                       static_cast<std::uint8_t>(clamp_u8(150 + (x % 48) - 24 + blob / 2)));
            bitmap.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 2,
                       static_cast<std::uint8_t>(clamp_u8(90 + (y % 64) - check)));
        }
    }
    return bitmap;
}

bool write_file(const char* path, const std::string& data) {
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const std::size_t n = std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    return n == data.size();
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 8 && argc != 9 && argc != 10) {
        std::fprintf(stderr,
                     "usage: av1_frame_probe w h q rdo_mode rdo_partition ivf yuv [flat] [prediction_only]\n");
        return 2;
    }
    const int w = std::atoi(argv[1]), h = std::atoi(argv[2]);
    Av1FrameEncodeOptions options;
    options.base_q_idx = std::atoi(argv[3]);
    options.rdo_mode = std::atoi(argv[4]) != 0;
    options.rdo_partition = std::atoi(argv[5]) != 0;
    if (argc == 10) options.prediction_only = std::atoi(argv[9]) != 0;

    Av1FrameYuv expected;
    const std::string ivf =
        encode_av1_ivf(make_texture(w, h, argc == 9 && std::atoi(argv[8]) != 0), options,
                       &expected);
    if (ivf.empty() || expected.y.empty()) return 1;

    std::string yuv;
    yuv.append(reinterpret_cast<const char*>(expected.y.data()), expected.y.size());
    yuv.append(reinterpret_cast<const char*>(expected.u.data()), expected.u.size());
    yuv.append(reinterpret_cast<const char*>(expected.v.data()), expected.v.size());
    if (!write_file(argv[6], ivf) || !write_file(argv[7], yuv)) return 1;
    return 0;
}
