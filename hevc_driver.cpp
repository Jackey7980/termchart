// Minimal driver to generate tc_hevc.265 using the project's HEVC encoder.
// Usage: hevc_driver [W H frames output]  (default 64 48 3 tc_hevc.265,
// which is the diag_hevc.py golden baseline and must stay byte-identical).
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "termchart/image/bitmap.hpp"
#include "termchart/image/hevc_encode.hpp"

using namespace termchart;
using namespace termchart::image;

Bitmap make_frame(int w, int h, int phase) {
    Bitmap bm(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), 3);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            bm.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 0,
                   static_cast<std::uint8_t>(x * 255 / w));
            bm.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 1,
                   static_cast<std::uint8_t>(y * 255 / h));
            bm.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 2,
                   static_cast<std::uint8_t>((phase * 40) % 256));
        }
    return bm;
}

int main(int argc, char** argv) {
    const int W = argc > 1 ? std::atoi(argv[1]) : 64;
    const int H = argc > 2 ? std::atoi(argv[2]) : 48;
    const int N = argc > 3 ? std::atoi(argv[3]) : 3;
    const char* out = argc > 4 ? argv[4] : "tc_hevc.265";
    std::vector<Bitmap> frames;
    frames.reserve(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) frames.push_back(make_frame(W, H, i));
    auto annexb = encode_hevc_annexb(frames, {});
    std::FILE* f = std::fopen(out, "wb");
    if (f) {
        std::fwrite(annexb.data(), 1, annexb.size(), f);
        std::fclose(f);
    }
    std::printf("%s: %zu bytes (%dx%d, %d frames)\n", out, annexb.size(), W, H, N);
    return 0;
}
