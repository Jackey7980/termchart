#include "inter_predict.hpp"

#include "warp_filter.hpp"

#include <algorithm>

#include "termchart/media/encoder/gpa/bit_ops.hpp"

namespace termchart::image::codec::av1 {
namespace {

int clip_pixel(int v) { return termchart::media::encoder::gpa::clip_pixel(v); }

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

// dav1d 的 3+FILTER 窄块表（宽或高 <= 4）：系数按 1/64 给出，这里统一乘 2
// 变成与主表相同的 1/128 定点，保持 (sum + 1024) >> 11 的取整模型。
constexpr std::int16_t kRegularSmall[15][8] = {
    {0, 0, -4, 126, 8, -2, 0, 0},    {0, 0, -8, 122, 18, -4, 0, 0},
    {0, 0, -10, 116, 28, -6, 0, 0},  {0, 0, -12, 110, 38, -8, 0, 0},
    {0, 0, -12, 102, 48, -10, 0, 0}, {0, 0, -14, 94, 58, -10, 0, 0},
    {0, 0, -12, 84, 66, -10, 0, 0},  {0, 0, -12, 76, 76, -12, 0, 0},
    {0, 0, -10, 66, 84, -12, 0, 0},  {0, 0, -10, 58, 94, -14, 0, 0},
    {0, 0, -10, 48, 102, -12, 0, 0}, {0, 0, -8, 38, 110, -12, 0, 0},
    {0, 0, -6, 28, 116, -10, 0, 0},  {0, 0, -4, 18, 122, -8, 0, 0},
    {0, 0, -2, 8, 126, -4, 0, 0},
};
constexpr std::int16_t kSmoothSmall[15][8] = {
    {0, 0, 30, 62, 34, 2, 0, 0},  {0, 0, 26, 62, 36, 4, 0, 0},
    {0, 0, 22, 62, 40, 4, 0, 0},  {0, 0, 20, 60, 42, 6, 0, 0},
    {0, 0, 18, 58, 44, 8, 0, 0},  {0, 0, 16, 56, 46, 10, 0, 0},
    {0, 0, 14, 54, 48, 12, 0, 0}, {0, 0, 12, 52, 52, 12, 0, 0},
    {0, 0, 12, 48, 54, 14, 0, 0}, {0, 0, 10, 46, 56, 16, 0, 0},
    {0, 0, 8, 44, 58, 18, 0, 0},  {0, 0, 6, 42, 60, 20, 0, 0},
    {0, 0, 4, 40, 62, 22, 0, 0},  {0, 0, 4, 36, 62, 26, 0, 0},
    {0, 0, 2, 34, 62, 30, 0, 0},
};
constexpr std::int16_t kSharpSmall[15][8] = {
    {-2, 2, -6, 126, 8, -2, 2, 0},     {-2, 6, -12, 124, 16, -6, 4, -2},
    {-2, 8, -18, 120, 26, -10, 6, -2}, {-4, 10, -22, 116, 38, -14, 6, -2},
    {-4, 10, -22, 108, 48, -18, 8, -2},{-4, 10, -24, 100, 60, -20, 8, -2},
    {-4, 10, -24, 90, 70, -22, 10, -2},{-2, 12, -24, 80, 80, -24, 12, -2},
    {-2, 10, -22, 70, 90, -24, 10, -4},{-2, 8, -20, 60, 100, -24, 10, -4},
    {-2, 8, -18, 48, 108, -22, 10, -4},{-2, 6, -14, 38, 116, -22, 10, -4},
    {-2, 6, -10, 26, 120, -18, 8, -2}, {-2, 4, -6, 16, 124, -12, 6, -2},
    {0, 2, -2, 8, 126, -6, 2, -2},
};

int clamp_coord(int v, int limit) { return std::clamp(v, 0, limit - 1); }

}  // namespace

const std::int16_t* interp_filter_taps(InterpFilter filter, int phase,
                                       bool small) {
    const int idx = phase & 15;
    if (small && idx != 0) {
        // 窄块表只有 15 相（对应 1/16..15/16），相位 0 仍是单位冲激。
        const int s = idx - 1;
        switch (filter) {
            case InterpFilter::Smooth: return kSmoothSmall[s];
            case InterpFilter::Sharp: return kRegularSmall[s];
            case InterpFilter::Bilinear: return kBilinear[idx];
            case InterpFilter::Regular:
            default: return kRegularSmall[s];
        }
    }
    switch (filter) {
        case InterpFilter::Smooth: return kSmooth[idx];
        case InterpFilter::Sharp: return kSharp[idx];
        case InterpFilter::Bilinear: return kBilinear[idx];
        case InterpFilter::Regular:
        default: return kRegular[idx];
    }
}

void inter_predict_block_phase(const std::uint8_t* ref, int ref_stride,
                               int ref_width, int ref_height,
                               std::uint8_t* dst, int dst_stride,
                               int w, int h, int x0, int y0,
                               int phase_x, int phase_y,
                               InterpFilter filter, bool small_h,
                               bool small_v) {
    const bool has_h = (phase_x & 15) != 0;
    const std::int16_t* h_taps = interp_filter_taps(filter, phase_x, small_h);
    const std::int16_t* v_taps = interp_filter_taps(filter, phase_y, small_v);
    // 水平结果统一保留 4 位小数（16 倍精度），纵向只取整一次：
    // 这就是 libaom av1_convolve_2d_sr_c / dav1d put_8tap_c 的取整模型，
    // 也是规范 7.11.3 对 x/y 双向都做亚像素时的中间精度。
    // 滚动保存源行 (y0+y-3)..(y0+y+4) 的水平滤波结果，滑块 (y+8)&7 是即将淘汰的行。
    int mid[8][16];  // 8 行环形缓冲（纵向抽头窗口只有 7 行），宽度按 16x16 块预留
    auto filter_row = [&](int k) {
        const int sy = clamp_coord(y0 - 3 + k, ref_height);
        const std::uint8_t* src = ref + sy * ref_stride;
        int* out = mid[k & 7];
        if (!has_h) {
            for (int x = 0; x < w; ++x)
                out[x] = src[clamp_coord(x0 + x, ref_width)] << 4;
            return;
        }
        for (int x = 0; x < w; ++x) {
            int sum = 0;
            for (int t = 0; t < 8; ++t)
                sum += src[clamp_coord(x0 + x - 3 + t, ref_width)] * h_taps[t];
            out[x] = (sum + 4) >> 3;
        }
    };
    for (int k = 0; k < 8; ++k) filter_row(k);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sum = 0;
            for (int t = 0; t < 8; ++t) sum += mid[(y + t) & 7][x] * v_taps[t];
            // 中间量 16 倍 + 纵向系数合计 128 ⇒ 归一化位移 11、偏置 1024。
            // 两个方向都是整数时相位为单位冲激，上式退化成原样拷贝。
            dst[y * dst_stride + x] =
                static_cast<std::uint8_t>(clip_pixel((sum + 1024) >> 11));
        }
        if (y + 1 < h) filter_row(y + 8);
    }
}

void inter_predict_block(const std::uint8_t* ref, int ref_stride,
                         int ref_width, int ref_height,
                         std::uint8_t* dst, int dst_stride,
                         int w, int h, int mv_x, int mv_y,
                         InterpFilter filter) {
    inter_predict_block_phase(ref, ref_stride, ref_width, ref_height,
                              dst, dst_stride, w, h, mv_x >> 3, mv_y >> 3,
                              (mv_x & 7) * 2, (mv_y & 7) * 2, filter);
}

void inter_predict_block_mid(const std::uint8_t* ref, int ref_stride,
                             int ref_width, int ref_height,
                             std::int16_t* dst, int dst_stride,
                             int w, int h, int x0, int y0,
                             int phase_x, int phase_y,
                             InterpFilter filter) {
    const bool has_h = (phase_x & 15) != 0;
    const std::int16_t* h_taps = interp_filter_taps(filter, phase_x);
    const std::int16_t* v_taps = interp_filter_taps(filter, phase_y);
    int mid[8][16];  // 8 行环形缓冲（纵向抽头窗口只有 7 行），宽度按 16x16 块预留
    auto filter_row = [&](int k) {
        const int sy = clamp_coord(y0 - 3 + k, ref_height);
        const std::uint8_t* src = ref + sy * ref_stride;
        int* out = mid[k & 7];
        if (!has_h) {
            for (int x = 0; x < w; ++x)
                out[x] = src[clamp_coord(x0 + x, ref_width)] << 4;
            return;
        }
        for (int x = 0; x < w; ++x) {
            int sum = 0;
            for (int t = 0; t < 8; ++t)
                sum += src[clamp_coord(x0 + x - 3 + t, ref_width)] * h_taps[t];
            out[x] = (sum + 4) >> 3;
        }
    };
    for (int k = 0; k < 8; ++k) filter_row(k);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sum = 0;
            for (int t = 0; t < 8; ++t) sum += mid[(y + t) & 7][x] * v_taps[t];
            // 复合路径的中间量：4 位小数（值域 0..4080），归一化位移 7、偏置 64。
            dst[y * dst_stride + x] = static_cast<std::int16_t>((sum + 64) >> 7);
        }
        if (y + 1 < h) filter_row(y + 8);
    }
}

void inter_compound_average_mid(const std::int16_t* a, const std::int16_t* b,
                                std::uint8_t* dst, int n) {
    for (int i = 0; i < n; ++i)
        dst[i] = static_cast<std::uint8_t>(clip_pixel((a[i] + b[i] + 16) >> 5));
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

void warp_affine_8x8(const std::uint8_t* ref, int ref_stride,
                     int ref_width, int ref_height,
                     std::uint8_t* dst, int dst_stride,
                     int x0, int y0, const std::int32_t matrix[6],
                     int alpha, int beta, int gamma, int delta,
                     int coord_scale) {
    const int cx = (x0 + 4) * coord_scale, cy = (y0 + 4) * coord_scale;
    // 坐标仍是 16.16 定点；色度平面先除以 2 回到色度像素单位。
    const int mvx = static_cast<int>((matrix[2] * static_cast<std::int64_t>(cx) +
                                      matrix[3] * static_cast<std::int64_t>(cy) +
                                      matrix[0]) >> (coord_scale - 1));
    const int mvy = static_cast<int>((matrix[4] * static_cast<std::int64_t>(cx) +
                                      matrix[5] * static_cast<std::int64_t>(cy) +
                                      matrix[1]) >> (coord_scale - 1));
    const int dx = (mvx >> 16) - 4;
    const int dy = (mvy >> 16) - 4;
    int mx = ((mvx & 0xFFFF) - alpha * 4 - beta * 7) & ~0x3F;
    int my = ((mvy & 0xFFFF) - gamma * 4 - delta * 4) & ~0x3F;

    std::int16_t mid[15][8];
    for (int k = 0; k < 15; ++k) {
        const int sy = clamp_coord(dy - 3 + k, ref_height);
        const std::uint8_t* src = ref + sy * ref_stride;
        for (int x = 0; x < 8; ++x) {
            const int tmx = mx + x * alpha;
            const std::int8_t* f = kWarpFilter[64 + ((tmx + 512) >> 10)];
            int sum = 0;
            for (int t = 0; t < 8; ++t)
                sum += src[clamp_coord(dx - 3 + x + t, ref_width)] * f[t];
            mid[k][x] = static_cast<std::int16_t>((sum + 4) >> 3);
        }
        mx += beta;
    }
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const int tmy = my + x * gamma;
            const std::int8_t* f = kWarpFilter[64 + ((tmy + 512) >> 10)];
            int sum = 0;
            // 8 抽头以 mid[y+3] 为中心：tap t 取 mid[y+t]（与滤波宏的 -3 偏移对齐）。
            for (int t = 0; t < 8; ++t) sum += mid[y + t][x] * f[t];
            dst[y * dst_stride + x] =
                static_cast<std::uint8_t>(clip_pixel((sum + 1024) >> 11));
        }
        my += delta;
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

void inter_obmc_blend_top(std::uint8_t* dst, int dst_stride,
                          const std::uint8_t* neighbor, int neighbor_stride,
                          int w, int h_px) {
    // 掩码与 dav1d_obmc_masks 逐字节一致：按「重叠区像素高」分段（2/4/8/16/32），
    // 段内第 y 行取 kObmcMask[h_px + y]。h_px 是重叠区高度（半个块高，上限 32），
    // 不是块高——dav1d 的 oh4 = imin(b_dim[1],16)>>1 就是这一步。
    static constexpr std::uint8_t kObmcMask[64] = {
         0,  0,
        19,  0,
        25, 14,  5,  0,
        28, 22, 16, 11,  7,  3,  0,  0,
        30, 27, 24, 21, 18, 15, 12, 10,  8,  6,  4,  3,  0,  0,  0,  0,
        31, 29, 28, 26, 24, 23, 21, 20, 19, 17, 16, 14, 13, 12, 11,  9,
         8,  7,  6,  5,  4,  4,  3,  2,  0,  0,  0,  0,  0,  0,  0,  0
    };
    if (h_px < 2 || h_px > 32) return;
    const int rows = (h_px * 3) >> 2;
    const std::uint8_t* mask = kObmcMask + h_px;
    for (int y = 0; y < rows; ++y) {
        const int m = mask[y];
        for (int x = 0; x < w; ++x) {
            dst[y * dst_stride + x] = static_cast<std::uint8_t>(clip_pixel(
                (dst[y * dst_stride + x] * (64 - m) +
                 neighbor[y * neighbor_stride + x] * m + 32) >> 6));
        }
    }
}

void inter_predict_chroma(const std::uint8_t* u, const std::uint8_t* v,
                          int ref_stride, int ref_w, int ref_h,
                          std::uint8_t* dst_u, std::uint8_t* dst_v,
                          int dst_stride, int w, int h, int mv_x, int mv_y,
                          InterpFilter filter) {
    // 4:2:0：一个色度样点 = 两个亮度样点，于是亮度的 1/8 样点步进正好是色度的
    // 1/16 样点相位——色度相位取亮度 MV 低 4 位，整数偏移取 mv >> 4。
    const int cx = mv_x >> 4, cy = mv_y >> 4;
    const int px = mv_x & 15, py = mv_y & 15;
    // 4x4 色度走 dav1d 的窄块（3+FILTER）系数表；宽高 <= 4 时参考实现选表不同。
    const bool small = w <= 4 && h <= 4;
    inter_predict_block_phase(u, ref_stride, ref_w, ref_h, dst_u, dst_stride,
                              w, h, cx, cy, px, py, filter, small, small);
    inter_predict_block_phase(v, ref_stride, ref_w, ref_h, dst_v, dst_stride,
                              w, h, cx, cy, px, py, filter, small, small);
}

namespace {

// dav1d warpmv.c 的定长倒数表（257 项，末项是 8192 的哨兵）。
constexpr std::int16_t kWarpDivLut[257] = {
    16384, 16320, 16257, 16194, 16132, 16070, 16009, 15948, 15888, 15828, 15768,
    15709, 15650, 15592, 15534, 15477, 15420, 15364, 15308, 15252, 15197, 15142,
    15087, 15033, 14980, 14926, 14873, 14821, 14769, 14717, 14665, 14614, 14564,
    14513, 14463, 14413, 14364, 14315, 14266, 14218, 14170, 14122, 14075, 14028,
    13981, 13935, 13888, 13843, 13797, 13752, 13707, 13662, 13618, 13574, 13530,
    13487, 13443, 13400, 13358, 13315, 13273, 13231, 13190, 13148, 13107, 13066,
    13026, 12985, 12945, 12906, 12866, 12827, 12788, 12749, 12710, 12672, 12633,
    12596, 12558, 12520, 12483, 12446, 12409, 12373, 12336, 12300, 12264, 12228,
    12193, 12157, 12122, 12087, 12053, 12018, 11984, 11950, 11916, 11882, 11848,
    11815, 11782, 11749, 11716, 11683, 11651, 11619, 11586, 11555, 11523, 11491,
    11460, 11429, 11398, 11367, 11336, 11305, 11275, 11245, 11215, 11185, 11155,
    11125, 11096, 11067, 11038, 11009, 10980, 10951, 10923, 10894, 10866, 10838,
    10810, 10782, 10755, 10727, 10700, 10673, 10645, 10618, 10592, 10565, 10538,
    10512, 10486, 10460, 10434, 10408, 10382, 10356, 10331, 10305, 10280, 10255,
    10230, 10205, 10180, 10156, 10131, 10107, 10082, 10058, 10034, 10010,  9986,
     9963,  9939,  9916,  9892,  9869,  9846,  9823,  9800,  9777,  9754,  9732,
     9709,  9687,  9664,  9642,  9620,  9598,  9576,  9554,  9533,  9511,  9489,
     9468,  9447,  9425,  9404,  9383,  9362,  9341,  9321,  9300,  9279,  9259,
     9239,  9218,  9198,  9178,  9158,  9138,  9118,  9098,  9079,  9059,  9039,
     9020,  9001,  8981,  8962,  8943,  8924,  8905,  8886,  8867,  8849,  8830,
     8812,  8793,  8775,  8756,  8738,  8720,  8702,  8684,  8666,  8648,  8630,
     8613,  8595,  8577,  8560,  8542,  8525,  8508,  8490,  8473,  8456,  8439,
     8422,  8405,  8389,  8372,  8355,  8339,  8322,  8306,  8289,  8273,  8257,
     8240,  8224,  8208,  8192,
};

int warp_clip(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

int warp_abs(int v) { return v < 0 ? -v : v; }

int warp_apply_sign(int v, int sign) { return sign < 0 ? -v : v; }

std::int64_t warp_llabs(std::int64_t v) { return v < 0 ? -v : v; }

int warp_apply_sign64(int v, std::int64_t sign) {
    return sign < 0 ? -v : v;
}

// 16 位定点剪切参数的取整裁剪：|cv| + 32 右移 6 位后再乘回 2^6。
int iclip_wmp(int v) {
    const int cv = warp_clip(v, -32768, 32767);
    return warp_apply_sign((warp_abs(cv) + 32) >> 6, cv) * (1 << 6);
}

int ulog2(std::uint32_t v) {
    int n = 0;
    while (v >>= 1) ++n;
    return n;
}

int u64log2(std::uint64_t v) {
    int n = 0;
    while (v >>= 1) ++n;
    return n;
}

// 把除数拆成「2 的幂 + 查表倍数」，与 dav1d 完全同构，保证末位一致。
int warp_resolve_divisor32(unsigned d, int* shift) {
    *shift = ulog2(d);
    const int e = static_cast<int>(d - (1u << *shift));
    const int f = *shift > 8 ? (e + (1 << (*shift - 9))) >> (*shift - 8)
                             : e << (8 - *shift);
    *shift += 14;
    return kWarpDivLut[f];
}

int warp_resolve_divisor64(std::uint64_t d, int* shift) {
    *shift = u64log2(d);
    const std::int64_t e = static_cast<std::int64_t>(d - (1ull << *shift));
    const std::int64_t f = *shift > 8 ? (e + (1ll << (*shift - 9))) >> (*shift - 8)
                                      : e << (8 - *shift);
    *shift += 14;
    return kWarpDivLut[static_cast<int>(f)];
}

int get_mult_shift_ndiag(std::int64_t px, int idet, int shift) {
    const std::int64_t v1 = px * idet;
    const int v2 = warp_apply_sign64(
        static_cast<int>((warp_llabs(v1) + ((1ll << shift) >> 1)) >> shift), v1);
    return warp_clip(v2, -0x1fff, 0x1fff);
}

int get_mult_shift_diag(std::int64_t px, int idet, int shift) {
    const std::int64_t v1 = px * idet;
    const int v2 = warp_apply_sign64(
        static_cast<int>((warp_llabs(v1) + ((1ll << shift) >> 1)) >> shift), v1);
    return warp_clip(v2, 0xe001, 0x11fff);
}

// 最小二乘解出 2x3 矩阵的后四元，再由块中心 MV 回填平移列。
int find_affine_int(const int (*pts)[2][2], int np, int bw4, int bh4,
                    int mv_x, int mv_y, std::int32_t* mat, int bx4, int by4) {
    int a[2][2] = {{0, 0}, {0, 0}};
    int bx[2] = {0, 0};
    int by[2] = {0, 0};
    const int rsuy = 2 * bh4 - 1;
    const int rsux = 2 * bw4 - 1;
    const int suy = rsuy * 8;
    const int sux = rsux * 8;
    const int duy = suy + mv_y;
    const int dux = sux + mv_x;
    const int isuy = by4 * 4 + rsuy;
    const int isux = bx4 * 4 + rsux;

    for (int i = 0; i < np; ++i) {
        const int dx = pts[i][1][0] - dux;
        const int dy = pts[i][1][1] - duy;
        const int sx = pts[i][0][0] - sux;
        const int sy = pts[i][0][1] - suy;
        if (warp_abs(sx - dx) < 256 && warp_abs(sy - dy) < 256) {
            a[0][0] += ((sx * sx) >> 2) + sx * 2 + 8;
            a[0][1] += ((sx * sy) >> 2) + sx + sy + 4;
            a[1][1] += ((sy * sy) >> 2) + sy * 2 + 8;
            bx[0] += ((sx * dx) >> 2) + sx + dx + 8;
            bx[1] += ((sy * dx) >> 2) + sy + dx + 4;
            by[0] += ((sx * dy) >> 2) + sx + dy + 4;
            by[1] += ((sy * dy) >> 2) + sy + dy + 8;
        }
    }

    const std::int64_t det =
        static_cast<std::int64_t>(a[0][0]) * a[1][1] -
        static_cast<std::int64_t>(a[0][1]) * a[0][1];
    if (det == 0) return 1;
    int shift = 0;
    int idet = warp_apply_sign64(
        warp_resolve_divisor64(static_cast<std::uint64_t>(warp_llabs(det)), &shift), det);
    shift -= 16;
    if (shift < 0) {
        idet <<= -shift;
        shift = 0;
    }

    mat[2] = get_mult_shift_diag(
        static_cast<std::int64_t>(a[1][1]) * bx[0] -
            static_cast<std::int64_t>(a[0][1]) * bx[1], idet, shift);
    mat[3] = get_mult_shift_ndiag(
        static_cast<std::int64_t>(a[0][0]) * bx[1] -
            static_cast<std::int64_t>(a[0][1]) * bx[0], idet, shift);
    mat[4] = get_mult_shift_ndiag(
        static_cast<std::int64_t>(a[1][1]) * by[0] -
            static_cast<std::int64_t>(a[0][1]) * by[1], idet, shift);
    mat[5] = get_mult_shift_diag(
        static_cast<std::int64_t>(a[0][0]) * by[1] -
            static_cast<std::int64_t>(a[0][1]) * by[0], idet, shift);

    mat[0] = warp_clip(mv_x * 0x2000 - (isux * (mat[2] - 0x10000) + isuy * mat[3]),
                       -0x800000, 0x7fffff);
    mat[1] = warp_clip(mv_y * 0x2000 - (isux * mat[4] + isuy * (mat[5] - 0x10000)),
                       -0x800000, 0x7fffff);
    return 0;
}

// 从矩阵反解 alpha/beta/gamma/delta；返回 true 表示剪切量越界（warp 不可用）。
bool shear_params_valid(std::int32_t* mat, WarpMotionParams* wm) {
    if (mat[2] <= 0) return true;

    wm->alpha = iclip_wmp(mat[2] - 0x10000);
    wm->beta = iclip_wmp(mat[3]);

    int shift = 0;
    const int y = warp_apply_sign(
        warp_resolve_divisor32(static_cast<unsigned>(warp_abs(mat[2])), &shift), mat[2]);
    const std::int64_t v1 = static_cast<std::int64_t>(mat[4]) * 0x10000 * y;
    const int rnd = (1 << shift) >> 1;
    wm->gamma = iclip_wmp(warp_apply_sign64(
        static_cast<int>((warp_llabs(v1) + rnd) >> shift), v1));
    const std::int64_t v2 = static_cast<std::int64_t>(mat[3]) * mat[4] * y;
    wm->delta = iclip_wmp(mat[5] - warp_apply_sign64(
                                     static_cast<int>((warp_llabs(v2) + rnd) >> shift), v2) -
                          0x10000);

    return (4 * warp_abs(wm->alpha) + 7 * warp_abs(wm->beta) >= 0x10000) ||
           (4 * warp_abs(wm->gamma) + 4 * warp_abs(wm->delta) >= 0x10000);
}

}  // namespace

WarpMotionParams derive_warp_motion(const WarpSample* samples, int np,
                                    int bw4, int bh4, int mv_x, int mv_y,
                                    int bx4, int by4) {
    WarpMotionParams wm;
    int pts[8][2][2] = {};
    const int thresh = 4 * warp_clip((bw4 > bh4 ? bw4 : bh4), 4, 28);

    // 每个采样点：邻块中心（规范记法 16*(2*dx + sx*bw4) - 8）与其 MV 指向的目标位置。
    for (int i = 0; i < np && i < 8; ++i) {
        pts[i][0][0] = 16 * (2 * samples[i].dx4 + samples[i].sx * samples[i].bw4) - 8;
        pts[i][0][1] = 16 * (2 * samples[i].dy4 + samples[i].sy * samples[i].bh4) - 8;
        pts[i][1][0] = pts[i][0][0] + samples[i].mv_x;
        pts[i][1][1] = pts[i][0][1] + samples[i].mv_y;
    }

    // 与当前块 MV 差超过阈值的邻居不参与拟合；全部被筛掉时退化用第一个采样点。
    int mvd[8];
    int ret = 0;
    for (int i = 0; i < np && i < 8; ++i) {
        mvd[i] = warp_abs(pts[i][1][0] - pts[i][0][0] - mv_x) +
                 warp_abs(pts[i][1][1] - pts[i][0][1] - mv_y);
        if (mvd[i] > thresh) {
            mvd[i] = -1;
        } else {
            ++ret;
        }
    }
    if (!ret) {
        ret = 1;
    } else {
        for (int i = 0, j = np - 1, k = 0; k < np - ret && i < 8 && j >= 0; ++k, ++i, --j) {
            while (i < 8 && mvd[i] != -1) ++i;
            while (j >= 0 && mvd[j] == -1) --j;
            if (i >= j) break;
            mvd[i] = mvd[j];
            pts[i][0][0] = pts[j][0][0];
            pts[i][0][1] = pts[j][0][1];
            pts[i][1][0] = pts[j][1][0];
            pts[i][1][1] = pts[j][1][1];
        }
    }

    if (find_affine_int(pts, ret, bw4, bh4, mv_x, mv_y, wm.matrix, bx4, by4) != 0) {
        return wm;
    }
    if (shear_params_valid(wm.matrix, &wm)) {
        return wm;
    }
    wm.affine = true;
    return wm;
}

}  // namespace termchart::image::codec::av1
