#include "inter_predict.hpp"

#include <algorithm>

namespace termchart::image::codec::av1 {
namespace {

int clip_pixel(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

// 规范 7.11.3 的 8 抽头滤波器，按 1/16 相位排列。
constexpr std::int16_t kRegular[16][8] = {
    {0, 0, 0, 128, 0, 0, 0, 0},      {0, 2, -6, 126, 8, -2, 0, 0},
    {0, 2, -10, 122, 18, -4, 0, 0},  {0, 2, -12, 116, 28, -8, 2, 0},
    {0, 2, -14, 110, 38, -10, 2, 0}, {0, 2, -14, 102, 48, -12, 2, 0},
    {0, 2, -16, 94, 58, -12, 2, 0},  {0, 2, -14, 84, 66, -12, 2, 0},
    {0, 2, -14, 76, 76, -14, 2, 0},  {0, 2, -12, 66, 84, -14, 2, 0},
    {0, 2, -12, 58, 94, -16, 2, 0},  {0, 2, -12, 48, 102, -14, 2, 0},
    {0, 2, -10, 38, 110, -14, 2, 0}, {0, 2, -8, 28, 116, -12, 2, 0},
    {0, 0, -4, 18, 122, -10, 2, 0},  {0, 0, -2, 8, 126, -6, 2, 0},
};
constexpr std::int16_t kSmooth[16][8] = {
    {0, 0, 0, 128, 0, 0, 0, 0},      {0, 2, 28, 62, 34, 2, 0, 0},
    {0, 0, 26, 62, 36, 4, 0, 0},     {0, 0, 22, 62, 40, 4, 0, 0},
    {0, 0, 20, 60, 42, 6, 0, 0},     {0, 0, 18, 58, 44, 8, 0, 0},
    {0, 0, 16, 56, 46, 10, 0, 0},    {0, -2, 16, 54, 48, 12, 0, 0},
    {0, -2, 14, 52, 52, 14, -2, 0},  {0, 0, 12, 48, 54, 16, -2, 0},
    {0, 0, 10, 46, 56, 16, 0, 0},    {0, 0, 8, 44, 58, 18, 0, 0},
    {0, 0, 6, 42, 60, 20, 0, 0},     {0, 0, 4, 40, 62, 22, 0, 0},
    {0, 0, 4, 36, 62, 26, 0, 0},     {0, 0, 2, 34, 62, 28, 2, 0},
};
constexpr std::int16_t kSharp[16][8] = {
    {0, 0, 0, 128, 0, 0, 0, 0},         {-2, 2, -6, 126, 8, -2, 2, 0},
    {-2, 6, -12, 124, 16, -6, 4, -2},   {-2, 8, -18, 120, 26, -10, 6, -2},
    {-4, 10, -22, 116, 38, -14, 6, -2}, {-4, 10, -22, 108, 48, -18, 8, -2},
    {-4, 10, -24, 100, 60, -20, 8, -2}, {-4, 10, -24, 90, 70, -22, 10, -2},
    {-4, 12, -24, 80, 80, -24, 12, -4}, {-2, 10, -22, 70, 90, -24, 10, -4},
    {-2, 8, -20, 60, 100, -24, 10, -4}, {-2, 8, -18, 48, 108, -22, 10, -4},
    {-2, 6, -14, 38, 116, -22, 10, -4}, {-2, 6, -10, 26, 120, -18, 8, -2},
    {-2, 4, -6, 16, 124, -12, 6, -2},   {0, 2, -2, 8, 126, -6, 2, -2},
};
constexpr std::int16_t kBilinear[16][8] = {
    {0, 0, 0, 128, 0, 0, 0, 0},  {0, 0, 0, 120, 8, 0, 0, 0},
    {0, 0, 0, 112, 16, 0, 0, 0}, {0, 0, 0, 104, 24, 0, 0, 0},
    {0, 0, 0, 96, 32, 0, 0, 0},  {0, 0, 0, 88, 40, 0, 0, 0},
    {0, 0, 0, 80, 48, 0, 0, 0},  {0, 0, 0, 72, 56, 0, 0, 0},
    {0, 0, 0, 64, 64, 0, 0, 0},  {0, 0, 0, 56, 72, 0, 0, 0},
    {0, 0, 0, 48, 80, 0, 0, 0},  {0, 0, 0, 40, 88, 0, 0, 0},
    {0, 0, 0, 32, 96, 0, 0, 0},  {0, 0, 0, 24, 104, 0, 0, 0},
    {0, 0, 0, 16, 112, 0, 0, 0}, {0, 0, 0, 8, 120, 0, 0, 0},
};

int clamp_coord(int v, int limit) { return std::clamp(v, 0, limit - 1); }

}  // namespace

const std::int16_t* interp_filter_taps(InterpFilter filter, int frac) {
    const int phase = (frac & 7) * 2;
    switch (filter) {
        case InterpFilter::Smooth: return kSmooth[phase];
        case InterpFilter::Sharp: return kSharp[phase];
        case InterpFilter::Bilinear: return kBilinear[phase];
        case InterpFilter::Regular:
        default: return kRegular[phase];
    }
}

void inter_predict_block(const std::uint8_t* ref, int ref_stride,
                         int ref_width, int ref_height,
                         std::uint8_t* dst, int dst_stride,
                         int w, int h, int mv_x, int mv_y,
                         InterpFilter filter) {
    const std::int16_t* h_taps = interp_filter_taps(filter, mv_x);
    const std::int16_t* v_taps = interp_filter_taps(filter, mv_y);
    const int x0 = mv_x >> 3, y0 = mv_y >> 3;
    for (int y = 0; y < h; ++y) {
        const int cy = y0 + y - 3;
        int row[8][8];
        for (int t = 0; t < 8; ++t) {
            const int sy = clamp_coord(cy + t, ref_height);
            for (int x = 0; x < w; ++x) {
                const int sx = x0 + x - 3;
                int sum = 0;
                for (int k = 0; k < 8; ++k)
                    sum += ref[sy * ref_stride + clamp_coord(sx + k, ref_width)] *
                           h_taps[k];
                row[t][x] = (sum + 64) >> 7;
            }
        }
        for (int x = 0; x < w; ++x) {
            int sum = 0;
            for (int k = 0; k < 8; ++k) sum += row[k][x] * v_taps[k];
            dst[y * dst_stride + x] =
                static_cast<std::uint8_t>(clip_pixel((sum + 64) >> 7));
        }
    }
}

void inter_compound_average(const std::uint8_t* a, const std::uint8_t* b,
                            std::uint8_t* dst, int n) {
    for (int i = 0; i < n; ++i)
        dst[i] = static_cast<std::uint8_t>((a[i] + b[i] + 1) >> 1);
}

void inter_compound_distance(const std::uint8_t* a, const std::uint8_t* b,
                             int weight, std::uint8_t* dst, int n) {
    weight = std::clamp(weight, -8, 8);
    for (int i = 0; i < n; ++i) {
        const int w0 = 8 - weight, w1 = 8 + weight;
        dst[i] = static_cast<std::uint8_t>(clip_pixel(
            (a[i] * w0 + b[i] * w1 + 8) >> 4));
    }
}

void warp_affine_predict(const std::uint8_t* ref, int ref_stride,
                         int ref_width, int ref_height,
                         std::uint8_t* dst, int dst_stride,
                         int x0, int y0, int w, int h,
                         const std::int32_t matrix[6]) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int sx = x0 + x, sy = y0 + y;
            const int src_x =
                (sx << 16) + (matrix[2] - 65536) * sx +
                matrix[3] * sy + matrix[0];
            const int src_y =
                (sy << 16) + (matrix[5] - 65536) * sy +
                matrix[4] * sx + matrix[1];
            const int ix = src_x >> 16, iy = src_y >> 16;
            const int fx = (src_x >> 8) & 255, fy = (src_y >> 8) & 255;
            const int x0i = clamp_coord(ix, ref_width);
            const int y0i = clamp_coord(iy, ref_height);
            const int x1i = clamp_coord(ix + 1, ref_width);
            const int y1i = clamp_coord(iy + 1, ref_height);
            const int a = ref[y0i * ref_stride + x0i] * (256 - fx) +
                          ref[y0i * ref_stride + x1i] * fx;
            const int b = ref[y1i * ref_stride + x0i] * (256 - fx) +
                          ref[y1i * ref_stride + x1i] * fx;
            dst[y * dst_stride + x] = static_cast<std::uint8_t>(
                clip_pixel(((a * (256 - fy) + b * fy) + 32768) >> 16));
        }
    }
}

void inter_obmc_blend(std::uint8_t* dst, int dst_stride,
                      const std::uint8_t* neighbor, int neighbor_stride,
                      const std::uint8_t weights[4][4], int w, int h) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int wt = weights[y & 3][x & 3];
            dst[y * dst_stride + x] = static_cast<std::uint8_t>(clip_pixel(
                (dst[y * dst_stride + x] * (32 - wt) +
                 neighbor[y * neighbor_stride + x] * wt + 16) >> 5));
        }
    }
}

void inter_predict_chroma(const std::uint8_t* u, const std::uint8_t* v,
                          int ref_stride, int ref_w, int ref_h,
                          std::uint8_t* dst_u, std::uint8_t* dst_v,
                          int dst_stride, int w, int h, int mv_x, int mv_y,
                          InterpFilter filter) {
    inter_predict_block(u, ref_stride, ref_w, ref_h, dst_u, dst_stride,
                        w, h, mv_x / 2, mv_y / 2, filter);
    inter_predict_block(v, ref_stride, ref_w, ref_h, dst_v, dst_stride,
                        w, h, mv_x / 2, mv_y / 2, filter);
}

}  // namespace termchart::image::codec::av1
