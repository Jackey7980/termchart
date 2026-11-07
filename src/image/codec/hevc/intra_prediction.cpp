// intra_prediction.cpp —— DC / Planar / V / H 帧内预测
#include "intra_prediction.hpp"

namespace termchart::image::codec::hevc {

void fill_reference(int n, const std::uint8_t* top, const std::uint8_t* left,
                    bool has_top, bool has_left, std::uint8_t* ref_top,
                    std::uint8_t* ref_left) {
    for (int i = 0; i <= n; ++i) {
        if (has_top) {
            ref_top[i] = top[i < n ? i : n - 1];  // 右上缺失时用最右上样本
        } else if (has_left) {
            ref_top[i] = left[0];
        } else {
            ref_top[i] = 128;
        }
        if (has_left) {
            ref_left[i] = left[i < n ? i : n - 1];
        } else if (has_top) {
            ref_left[i] = top[0];
        } else {
            ref_left[i] = 128;
        }
    }
}

void predict(IntraMode mode, int n, const std::uint8_t* top,
             const std::uint8_t* left, bool has_top, bool has_left,
             std::uint8_t* dst) {
    std::uint8_t t[64], l[64];
    fill_reference(n, top, left, has_top, has_left, t, l);

    if (mode == IntraMode::Vertical) {
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) dst[y * n + x] = t[x];
        return;
    }
    if (mode == IntraMode::Horizontal) {
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) dst[y * n + x] = l[y];
        return;
    }
    if (mode == IntraMode::Dc) {
        int sum = 0, cnt = 0;
        if (has_top) for (int i = 0; i < n; ++i) { sum += t[i]; ++cnt; }
        if (has_left) for (int i = 0; i < n; ++i) { sum += l[i]; ++cnt; }
        const int dc = cnt ? (sum + cnt / 2) / cnt : 128;
        for (int i = 0; i < n * n; ++i) dst[i] = static_cast<std::uint8_t>(dc);
        return;
    }
    // Planar（HEVC 8.4.4.2.4）：双线性插值，右上/左下样本参与。
    int shift = 1;
    while ((1 << shift) < n) ++shift;  // log2(n)
    const int top_right = t[n], bottom_left = l[n];
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const int v = (n - 1 - x) * l[y] + (x + 1) * top_right +
                          (n - 1 - y) * t[x] + (y + 1) * bottom_left + n;
            dst[y * n + x] = static_cast<std::uint8_t>(v >> (shift + 1));
        }
    }
}

}  // namespace termchart::image::codec::hevc
