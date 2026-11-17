#include "interp.hpp"

#include <algorithm>
#include <cstdint>

namespace termchart::image::codec::hevc {
namespace {

// FFmpeg libavcodec/hevcdsp.c 权威系数（HEVC 规范 Table 8-9/8-10，8bit 分支）。
// luma: ff_hevc_qpel_filters[frac-1][0..7]，frac∈1..3（frac 0 直通）。
// 注意 frac1/frac3 是【非对称】8-tap，不是早期猜测的对称 {-1,4,-10,58,58,...}。
constexpr std::int8_t kLumaFilter[3][8] = {
    {-1, 4, -10, 58, 17, -5, 1, 0},   // frac 1
    {-1, 4, -11, 40, 40, -11, 4, -1}, // frac 2
    {0, 1, -5, 17, 58, -10, 4, -1},   // frac 3
};

// chroma: ff_hevc_epel_filters[frac-1][0..3]，frac∈1..7（1/8 精度）。
// 4 抽头含负值，**不是**两抽头线性；系数和恒 64，直接 (Σ+32)>>6。
constexpr std::int8_t kChromaFilter[7][4] = {
    {-2, 58, 10, -2},  // frac 1 (1/8)
    {-4, 54, 16, -2},  // frac 2 (2/8)
    {-6, 46, 28, -4},  // frac 3 (3/8)
    {-4, 36, 36, -4},  // frac 4 (4/8)
    {-4, 28, 46, -6},  // frac 5 (5/8)
    {-2, 16, 54, -4},  // frac 6 (6/8)
    {-2, 10, 58, -2},  // frac 7 (7/8)
};

constexpr int clip8(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

// 取一个越界按边缘复制的样本（等价 emulated_edge_mc 的单点语义）。
inline std::uint8_t sample(const std::uint8_t* ref, int w, int h, int x, int y) {
    const int cx = std::clamp(x, 0, w - 1);
    const int cy = std::clamp(y, 0, h - 1);
    return ref[static_cast<std::size_t>(cy) * static_cast<std::size_t>(w) +
               static_cast<std::size_t>(cx)];
}

// 水平 8-tap：返回 int（不 clip），供两级滤波的中间趟使用。
int filter_h_luma(const std::uint8_t* ref, int w, int h, int x, int y, const std::int8_t* f) {
    int acc = 0;
    for (int k = 0; k < 8; ++k) {
        acc += static_cast<int>(f[k]) * static_cast<int>(sample(ref, w, h, x + k - 3, y));
    }
    return acc;
}

}  // namespace

void interp_luma(const std::uint8_t* ref, int ref_w, int ref_h, int x_int, int y_int,
                 int frac_x, int frac_y, int w, int h, std::uint8_t* dst, int dst_stride) {
    const bool hf = frac_x != 0;
    const bool vf = frac_y != 0;
    if (!hf && !vf) {
        for (int j = 0; j < h; ++j) {
            std::uint8_t* d = dst + static_cast<std::size_t>(j) * dst_stride;
            for (int i = 0; i < w; ++i) d[i] = sample(ref, ref_w, ref_h, x_int + i, y_int + j);
        }
        return;
    }

    const std::int8_t* fx = hf ? kLumaFilter[frac_x - 1] : nullptr;
    const std::int8_t* fy = vf ? kLumaFilter[frac_y - 1] : nullptr;

    if (!vf) {
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i)
                dst[static_cast<std::size_t>(j) * dst_stride + i] =
                    static_cast<std::uint8_t>(clip8((filter_h_luma(ref, ref_w, ref_h, x_int + i,
                                                                    y_int + j, fx) + 32) >> 6));
        return;
    }
    if (!hf) {
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i) {
                int acc = 0;
                for (int k = 0; k < 8; ++k)
                    acc += static_cast<int>(fy[k]) *
                           static_cast<int>(sample(ref, ref_w, ref_h, x_int + i, y_int + j + k - 3));
                dst[static_cast<std::size_t>(j) * dst_stride + i] =
                    static_cast<std::uint8_t>(clip8((acc + 32) >> 6));
            }
        return;
    }

    // hv（对齐 FFmpeg put_hevc_qpel_uni_hv，8bit）：水平趟保留 int16（不 clip/不右移），
    // 垂直趟 clip(((Σv >> 6) + 32) >> 6) —— 注意中间先 >>6 再加 offset，两次舍入。
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) {
            int tmp[8] = {};
            for (int t = 0; t < 8; ++t)
                tmp[t] = filter_h_luma(ref, ref_w, ref_h, x_int + i, y_int + j + t - 3, fx);
            int acc = 0;
            for (int k = 0; k < 8; ++k) acc += static_cast<int>(fy[k]) * tmp[k];
            dst[static_cast<std::size_t>(j) * dst_stride + i] =
                static_cast<std::uint8_t>(clip8(((acc >> 6) + 32) >> 6));
        }
}

void interp_chroma(const std::uint8_t* ref, int ref_w, int ref_h, int x_int, int y_int,
                   int frac_x, int frac_y, int w, int h, std::uint8_t* dst, int dst_stride) {
    const bool hf = frac_x != 0;
    const bool vf = frac_y != 0;
    if (!hf && !vf) {
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i)
                dst[static_cast<std::size_t>(j) * dst_stride + i] =
                    sample(ref, ref_w, ref_h, x_int + i, y_int + j);
        return;
    }
    // 4-tap：抽头 x-1 .. x+2，系数和恒 64，直接 (Σ+32)>>6。
    auto fh = [&](int x, int y, const std::int8_t* f) {
        int acc = 0;
        for (int k = 0; k < 4; ++k)
            acc += static_cast<int>(f[k]) * static_cast<int>(sample(ref, ref_w, ref_h, x + k - 1, y));
        return acc;
    };
    const std::int8_t* fx = hf ? kChromaFilter[frac_x - 1] : nullptr;
    const std::int8_t* fy = vf ? kChromaFilter[frac_y - 1] : nullptr;
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) {
            if (hf && vf) {
                // epel uni hv：水平趟保留 int，垂直 clip(((Σ>>6)+32)>>6)（两次舍入）。
                int tmp[4] = {};
                for (int t = 0; t < 4; ++t)
                    tmp[t] = fh(x_int + i, y_int + j + t - 1, fx);
                int acc = 0;
                for (int k = 0; k < 4; ++k) acc += static_cast<int>(fy[k]) * tmp[k];
                dst[static_cast<std::size_t>(j) * dst_stride + i] =
                    static_cast<std::uint8_t>(clip8(((acc >> 6) + 32) >> 6));
            } else {
                int acc;
                if (hf) {
                    acc = fh(x_int + i, y_int + j, fx);
                } else {
                    acc = 0;
                    for (int k = 0; k < 4; ++k)
                        acc += static_cast<int>(fy[k]) *
                               static_cast<int>(sample(ref, ref_w, ref_h, x_int + i, y_int + j + k - 1));
                }
                dst[static_cast<std::size_t>(j) * dst_stride + i] =
                    static_cast<std::uint8_t>(clip8((acc + 32) >> 6));
            }
        }
}

}  // namespace termchart::image::codec::hevc
