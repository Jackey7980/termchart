// intra_prediction.cpp —— HEVC 4x4 帧内预测（Planar / DC）
//
// 逐式对齐 HEVC 规范 8.4.4.2.4（Planar）与 8.4.4.2.5（DC），保证编码端预测
// 与解码端（ffmpeg）完全一致。
#include "intra_prediction.hpp"

#include <cstring>

namespace termchart::image::codec::hevc {
namespace {

int log2_of(int n) {
    int k = 0;
    while ((1 << k) < n) ++k;
    return k;
}

void predict_planar(int n, const std::uint8_t* top, const std::uint8_t* left,
                    std::uint8_t* dst) {
    const int log2n = log2_of(n);
    const int shift = log2n + 1;  // log2(2n)
    const int topRight = top[n];   // p[n][-1]
    const int botLeft = left[n];   // p[-1][n]
    for (int y = 0; y < n; ++y) {
        const int ly = left[y];
        for (int x = 0; x < n; ++x) {
            const int v = (n - 1 - x) * ly + (x + 1) * topRight +
                          (n - 1 - y) * top[x] + (y + 1) * botLeft + n;
            dst[y * n + x] = static_cast<std::uint8_t>(v >> shift);
        }
    }
}

void predict_dc(int n, const std::uint8_t* top, const std::uint8_t* left,
                bool has_top, bool has_left, std::uint8_t* dst) {
    const int log2n = log2_of(n);
    int dc;
    if (has_top && has_left) {
        int sum = 0;
        for (int i = 0; i < n; ++i) sum += top[i] + left[i];
        dc = (sum + n) >> (log2n + 1);
    } else if (has_top) {
        int sum = 0;
        for (int i = 0; i < n; ++i) sum += top[i];
        dc = (sum + (n >> 1)) >> log2n;
    } else if (has_left) {
        int sum = 0;
        for (int i = 0; i < n; ++i) sum += left[i];
        dc = (sum + (n >> 1)) >> log2n;
    } else {
        dc = 128;  // 1 << (bitDepth - 1)
    }
    std::memset(dst, 0, static_cast<std::size_t>(n) * static_cast<std::size_t>(n));
    for (int i = 0; i < n * n; ++i) dst[i] = static_cast<std::uint8_t>(dc);
}

}  // namespace

void predict_intra(int mode, int n, const std::uint8_t* top,
                   const std::uint8_t* left, bool has_top, bool has_left,
                   std::uint8_t* dst) {
    if (mode == 0) {
        predict_planar(n, top, left, dst);
    } else {
        predict_dc(n, top, left, has_top, has_left, dst);
    }
}

}  // namespace termchart::image::codec::hevc
