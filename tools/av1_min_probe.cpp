// av1_min_probe.cpp —— dump 一期最小 AV1 IVF 流，供 ffmpeg/libdav1d 真解码
#include <cstdio>
#include <string>

#include "termchart/image/bitmap.hpp"
#include "av1_encode.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: av1_min_probe out.ivf\n");
        return 2;
    }
    termchart::image::Bitmap bmp(8, 8, 4);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            const auto v = static_cast<std::uint8_t>(128);
            bmp.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 0, v);
            bmp.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 1, v);
            bmp.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 2, v);
        }
    const std::string ivf = termchart::image::encode_av1_minimal_ivf(bmp);
    FILE* f = std::fopen(argv[1], "wb");
    if (!f) return 3;
    std::fwrite(ivf.data(), 1, ivf.size(), f);
    std::fclose(f);
    std::fprintf(stderr, "ivf bytes=%zu\n", ivf.size());
    return 0;
}
