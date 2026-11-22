#include "intra_edge.hpp"

#include <cstdlib>
#include <cstring>

namespace termchart::image::codec::av1 {
namespace {

int clip_pixel(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

// 角度 -> 1/256 单位的 tan 表；用不上的角度留 0。
constexpr std::int16_t kDrIntraDerivative[90] = {
    0,    0, 0,           //
    1023, 0, 0,           // 3
    547,  0, 0,           // 6
    372,  0, 0, 0, 0,     // 9
    273,  0, 0,           // 14
    215,  0, 0,           // 17
    178,  0, 0,           // 20
    151,  0, 0,           // 23
    132,  0, 0,           // 26
    116,  0, 0,           // 29
    102,  0, 0, 0,        // 32
    90,   0, 0,           // 36
    80,   0, 0,           // 39
    71,   0, 0,           // 42
    64,   0, 0,           // 45
    57,   0, 0,           // 48
    51,   0, 0,           // 51
    45,   0, 0, 0,        // 54
    40,   0, 0,           // 58
    35,   0, 0,           // 61
    31,   0, 0,           // 64
    27,   0, 0,           // 67
    23,   0, 0,           // 70
    19,   0, 0,           // 73
    15,   0, 0, 0, 0,     // 76
    11,   0, 0,           // 81
    7,    0, 0,           // 84
    3,    0, 0,           // 87
};

constexpr int kEdgeTaps = 5;

}  // namespace

int intra_edge_filter_strength(int bs0, int bs1, int delta, int type) {
    const int d = std::abs(delta);
    const int blk_wh = bs0 + bs1;
    int strength = 0;
    if (type == 0) {
        if (blk_wh <= 8) {
            if (d >= 56) strength = 1;
        } else if (blk_wh <= 12) {
            if (d >= 40) strength = 1;
        } else if (blk_wh <= 16) {
            if (d >= 40) strength = 1;
        } else if (blk_wh <= 24) {
            if (d >= 8) strength = 1;
            if (d >= 16) strength = 2;
            if (d >= 32) strength = 3;
        } else if (blk_wh <= 32) {
            if (d >= 1) strength = 1;
            if (d >= 4) strength = 2;
            if (d >= 32) strength = 3;
        } else {
            if (d >= 1) strength = 3;
        }
    } else {
        if (blk_wh <= 8) {
            if (d >= 40) strength = 1;
            if (d >= 64) strength = 2;
        } else if (blk_wh <= 16) {
            if (d >= 20) strength = 1;
            if (d >= 48) strength = 2;
        } else if (blk_wh <= 24) {
            if (d >= 4) strength = 3;
        } else {
            if (d >= 1) strength = 3;
        }
    }
    return strength;
}

int use_intra_edge_upsample(int bs0, int bs1, int delta, int type) {
    const int d = std::abs(delta);
    const int blk_wh = bs0 + bs1;
    if (d == 0 || d >= 40) return 0;
    return type ? (blk_wh <= 8) : (blk_wh <= 16);
}

void filter_intra_edge(std::uint8_t* p, int sz, int strength) {
    if (strength == 0 || sz <= 0) return;

    // 三档核，越往后越“平”。
    constexpr int kKernel[3][kEdgeTaps] = {{0, 4, 8, 4, 0},
                                           {0, 5, 6, 5, 0},
                                           {2, 4, 4, 4, 2}};
    const int filt = strength - 1;
    std::uint8_t edge[160];
    if (sz > static_cast<int>(sizeof(edge))) return;
    std::memcpy(edge, p, static_cast<std::size_t>(sz));

    for (int i = 1; i < sz; ++i) {
        int s = 0;
        for (int j = 0; j < kEdgeTaps; ++j) {
            int k = i - 2 + j;
            k = k < 0 ? 0 : k;
            k = k > sz - 1 ? sz - 1 : k;
            s += edge[k] * kKernel[filt][j];
        }
        p[i] = static_cast<std::uint8_t>((s + 8) >> 4);
    }
}

void filter_intra_edge_corner(std::uint8_t* above, std::uint8_t* left) {
    const int s = (left[0] * 5) + (above[-1] * 6) + (above[0] * 5);
    const auto v = static_cast<std::uint8_t>((s + 8) >> 4);
    above[-1] = v;
    left[-1] = v;
}

void upsample_intra_edge(std::uint8_t* p, int sz) {
    if (sz <= 0) return;

    std::uint8_t in[160];
    in[0] = p[-1];
    in[1] = p[-1];
    for (int i = 0; i < sz; ++i) in[i + 2] = p[i];
    in[sz + 2] = p[sz - 1];

    p[-2] = in[0];
    for (int i = 0; i < sz; ++i) {
        int s = -static_cast<int>(in[i]) + 9 * in[i + 1] + 9 * in[i + 2] -
                static_cast<int>(in[i + 3]);
        s = clip_pixel((s + 8) >> 4);
        p[2 * i - 1] = static_cast<std::uint8_t>(s);
        p[2 * i] = in[i + 2];
    }
}

int intra_dx(int angle) {
    if (angle > 0 && angle < 90) return kDrIntraDerivative[angle];
    if (angle > 90 && angle < 180) return kDrIntraDerivative[180 - angle];
    return 1;
}

int intra_dy(int angle) {
    if (angle > 90 && angle < 180) return kDrIntraDerivative[angle - 90];
    if (angle > 180 && angle < 270) return kDrIntraDerivative[270 - angle];
    return 1;
}

}  // namespace termchart::image::codec::av1
