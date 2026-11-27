#include "cdef.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

namespace termchart::image::codec::av1 {
namespace {

// 方向偏移以 12 行为单位（滤波缓冲 stride），与规范 cdef_directions 一致。
constexpr std::int8_t kDirections[12][2] = {
    {12, 24}, {12, 23}, {-11, -22}, {1, -10},
    {1, 2}, {1, 14}, {13, 26}, {12, 25},
    {12, 24}, {12, 23}, {-11, -22}, {1, -10},
};
constexpr int log2_floor(int v) {
    int n = 0;
    while (v > 1) { v >>= 1; ++n; }
    return n;
}
int constrain(int diff, int threshold, int shift) {
    const int a = std::abs(diff);
    const int v = std::min(a, std::max(0, threshold - (a >> shift)));
    return diff < 0 ? -v : v;
}

}  // namespace

int av1_cdef_find_dir(const std::uint8_t* img, int stride, unsigned* var) {
    int sum_hv[2][8] = {};
    int sum_diag[2][15] = {};
    int sum_alt[4][11] = {};
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const int px = static_cast<int>(img[y * stride + x]) - 128;
            sum_diag[0][y + x] += px;
            sum_alt[0][y + (x >> 1)] += px;
            sum_hv[0][y] += px;
            sum_alt[1][3 + y - (x >> 1)] += px;
            sum_diag[1][7 + y - x] += px;
            sum_alt[2][3 - (y >> 1) + x] += px;
            sum_hv[1][x] += px;
            sum_alt[3][(y >> 1) + x] += px;
        }
    }
    unsigned cost[8] = {};
    for (int n = 0; n < 8; ++n) {
        cost[2] += static_cast<unsigned>(sum_hv[0][n] * sum_hv[0][n]);
        cost[6] += static_cast<unsigned>(sum_hv[1][n] * sum_hv[1][n]);
    }
    cost[2] *= 105; cost[6] *= 105;
    constexpr int div_tab[7] = {840, 420, 280, 210, 168, 140, 120};
    for (int n = 0; n < 7; ++n) {
        const int d = div_tab[n];
        cost[0] += (sum_diag[0][n] * sum_diag[0][n] +
                    sum_diag[0][14 - n] * sum_diag[0][14 - n]) * d;
        cost[4] += (sum_diag[1][n] * sum_diag[1][n] +
                    sum_diag[1][14 - n] * sum_diag[1][14 - n]) * d;
    }
    cost[0] += sum_diag[0][7] * sum_diag[0][7] * 105;
    cost[4] += sum_diag[1][7] * sum_diag[1][7] * 105;
    for (int n = 0; n < 4; ++n) {
        unsigned& c = cost[n * 2 + 1];
        for (int m = 0; m < 5; ++m) c += sum_alt[n][3 + m] * sum_alt[n][3 + m];
        c *= 105;
        for (int m = 0; m < 3; ++m) {
            const int d = div_tab[2 * m + 1];
            c += (sum_alt[n][m] * sum_alt[n][m] +
                  sum_alt[n][10 - m] * sum_alt[n][10 - m]) * d;
        }
    }
    int best = 0;
    for (int n = 1; n < 8; ++n)
        if (cost[n] > cost[best]) best = n;
    *var = (cost[best] - cost[best ^ 4]) >> 10;
    return best;
}

void av1_cdef_filter_8x8(std::uint8_t* dst, int dst_stride,
                         const std::uint8_t* src, int src_stride,
                         int dir, const Av1CdefParams& params, int edges) {
    // 12x12 扩展缓冲：中心偏移 (2,2)，四边按帧边界钳制填充。
    std::int16_t tmp[12 * 12];
    const auto at = [&](int x, int y) -> int {
        const int cx = std::clamp(x, 0, 7), cy = std::clamp(y, 0, 7);
        (void)edges;
        return src[cy * src_stride + cx];
    };
    for (int y = -2; y < 10; ++y)
        for (int x = -2; x < 10; ++x)
            tmp[(y + 2) * 12 + (x + 2)] = static_cast<std::int16_t>(at(x, y));

    const int pri = std::clamp(params.pri_strength, 0, 31);
    const int sec = std::clamp(params.sec_strength, 0, 31);
    const int damping = std::clamp(params.damping, 3, 15);
    const int pri_tap = 4 - (pri & 1);
    const int pri_shift = pri ? std::max(0, damping - log2_floor(pri)) : 0;
    const int sec_shift = sec ? damping - log2_floor(sec) : 0;
    const int d = dir & 7;
    const auto dir_pair = [&](int table_idx) {
        return std::pair<int, int>{kDirections[table_idx][0],
                                   kDirections[table_idx][1]};
    };
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const int px = tmp[(y + 2) * 12 + x + 2];
            int sum = 0;
            if (pri) {
                int tap = pri_tap;
                for (int k = 0; k < 2; ++k) {
                    const auto p = dir_pair(2 + d);
                    const int off = k == 0 ? p.first : p.second;
                    const int base = (y + 2) * 12 + x + 2;
                    const int a = tmp[base + off];
                    const int b = tmp[base - off];
                    sum += tap * constrain(a - px, pri, pri_shift);
                    sum += tap * constrain(b - px, pri, pri_shift);
                    tap = (tap & 3) | 2;
                }
            }
            if (sec) {
                for (int k = 0; k < 2; ++k) {
                    const auto p1 = dir_pair(4 + d);
                    const auto p0 = dir_pair(d);
                    const int o1 = k == 0 ? p1.first : p1.second;
                    const int o0 = k == 0 ? p0.first : p0.second;
                    const int base = (y + 2) * 12 + x + 2;
                    const int v[4] = {
                        tmp[base + o1], tmp[base - o1],
                        tmp[base + o0], tmp[base - o0],
                    };
                    const int tap = 2 - k;
                    for (int j = 0; j < 4; ++j)
                        sum += tap * constrain(v[j] - px, sec, sec_shift);
                }
            }
            dst[y * dst_stride + x] = static_cast<std::uint8_t>(
                std::clamp(px + ((sum - (sum < 0) + 8) >> 4), 0, 255));
        }
    }
}

void av1_cdef_plane(std::uint8_t* plane, int stride, int w, int h,
                    const Av1CdefParams& params) {
    std::array<std::uint8_t, 64> block{}, backup{};
    for (int by = 0; by + 8 <= h; by += 8) {
        for (int bx = 0; bx + 8 <= w; bx += 8) {
            for (int y = 0; y < 8; ++y)
                std::memcpy(block.data() + y * 8, plane + (by + y) * stride + bx, 8);
            backup = block;
            unsigned var = 0;
            const int dir = av1_cdef_find_dir(block.data(), 8, &var);
            av1_cdef_filter_8x8(block.data(), 8, backup.data(), 8, dir, params,
                                (bx > 0 ? 1 : 0) | (bx + 8 < w ? 2 : 0) |
                                (by > 0 ? 4 : 0) | (by + 8 < h ? 8 : 0));
            for (int y = 0; y < 8; ++y)
                std::memcpy(plane + (by + y) * stride + bx, block.data() + y * 8, 8);
        }
    }
}

}  // namespace termchart::image::codec::av1
