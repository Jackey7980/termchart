#include "loop_restoration.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace termchart::image::codec::av1 {
namespace {

int at(const std::uint8_t* src, int stride, int w, int h, int x, int y) {
    return src[std::clamp(y, 0, h - 1) * stride + std::clamp(x, 0, w - 1)];
}

void box_filter(const std::uint8_t* src, int stride, int w, int h, int r,
                std::vector<int>& mean, std::vector<int>& var) {
    const int n = (2 * r + 1) * (2 * r + 1);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            long long sum = 0, sq = 0;
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx) {
                    const int v = at(src, stride, w, h, x + dx, y + dy);
                    sum += v;
                    sq += static_cast<long long>(v) * v;
                }
            const int m = static_cast<int>(sum / n);
            mean[static_cast<std::size_t>(y * w + x)] = m;
            var[static_cast<std::size_t>(y * w + x)] =
                static_cast<int>((sq - static_cast<long long>(m) * m * n) / n);
        }
    }
}

}  // namespace

void av1_lr_wiener_unit(std::uint8_t* dst, int dst_stride,
                        const std::uint8_t* src, int src_stride,
                        int w, int h, const Av1WienerParams& params) {
    if (w <= 0 || h <= 0) return;
    if (params.h[0] == 0 && params.h[1] == 0 && params.h[2] == 0 &&
        params.v[0] == 0 && params.v[1] == 0 && dst != src) {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                dst[y * dst_stride + x] = src[y * src_stride + x];
        return;
    }
    const int fh[7] = {params.h[0], params.h[1], params.h[2],
                       -2 * (params.h[0] + params.h[1] + params.h[2]),
                       params.h[2], params.h[1], params.h[0]};
    const int fv[5] = {params.v[0], params.v[1],
                       128 - 2 * (params.v[0] + params.v[1]),
                       params.v[1], params.v[0]};
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sum = 0;
            for (int k = 0; k < 5; ++k)
                for (int j = 0; j < 7; ++j)
                    sum += fv[k] * fh[j] *
                           at(src, src_stride, w, h, x + j - 3, y + k - 2);
            dst[y * dst_stride + x] = static_cast<std::uint8_t>(
                std::clamp((sum + (1 << 14)) >> 15, 0, 255));
        }
    }
}

void av1_lr_sgrproj_unit(std::uint8_t* dst, int dst_stride,
                         const std::uint8_t* src, int src_stride,
                         int w, int h, const Av1SgrParams& params) {
    if (w <= 0 || h <= 0) return;
    if (params.w0 == 0 && params.w1 == 0) {
        if (dst != src)
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    dst[y * dst_stride + x] = src[y * src_stride + x];
        return;
    }
    const int n = w * h;
    std::vector<int> m3(n), v3(n), m5(n), v5(n);
    if (params.w0) box_filter(src, src_stride, w, h, 2, m5, v5);
    if (params.w1) box_filter(src, src_stride, w, h, 1, m3, v3);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int i = y * w + x;
            const int p = src[y * src_stride + x];
            int out = p << 7;
            if (params.w0) {
                const int a = (v5[static_cast<std::size_t>(i)] * 156) >> 8;
                const int denom = params.s0 + v5[static_cast<std::size_t>(i)];
                const int aa = denom ? (a << 8) / denom : 0;
                const int bb = ((m5[static_cast<std::size_t>(i)] << 8) -
                                aa * m5[static_cast<std::size_t>(i)]) >> 8;
                out += params.w0 * std::clamp(((aa * p >> 8) + bb), 0, 255);
            }
            if (params.w1) {
                const int a = (v3[static_cast<std::size_t>(i)] * 156) >> 8;
                const int denom = params.s1 + v3[static_cast<std::size_t>(i)];
                const int aa = denom ? (a << 8) / denom : 0;
                const int bb = ((m3[static_cast<std::size_t>(i)] << 8) -
                                aa * m3[static_cast<std::size_t>(i)]) >> 8;
                out += params.w1 * std::clamp(((aa * p >> 8) + bb), 0, 255);
            }
            dst[y * dst_stride + x] = static_cast<std::uint8_t>(
                std::clamp((out + 64) >> 7, 0, 255));
        }
    }
}

}  // namespace termchart::image::codec::av1
