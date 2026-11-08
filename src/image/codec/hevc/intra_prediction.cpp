// intra_prediction.cpp —— HEVC 4x4 帧内预测（Planar / DC）
//
// 逐式对齐 HEVC 规范 8.4.4.2.4（Planar）与 8.4.4.2.5（DC），保证编码端预测
// 与解码端（ffmpeg）完全一致。
#include "intra_prediction.hpp"

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
    // HEVC 8.4.4.2.2 参考样本替代：某一侧不可用时，用另一侧第一个可用样本
    // 填充该侧；两侧都不可用时用 1 << (bitDepth-1) = 128。
    // 替代后的样本【同样参与 dcVal 的双侧求和】，不能退化成单侧平均
    // （那样 dcVal 会偏，且重建误差会沿预测链传播）。
    std::uint8_t t[32] = {}, l[32] = {};
    if (!has_top && !has_left) {
        for (int i = 0; i < n; ++i) t[i] = l[i] = 128;
    } else if (!has_top) {
        for (int i = 0; i < n; ++i) { t[i] = left[0]; l[i] = left[i]; }
    } else if (!has_left) {
        for (int i = 0; i < n; ++i) { t[i] = top[i]; l[i] = top[0]; }
    } else {
        for (int i = 0; i < n; ++i) { t[i] = top[i]; l[i] = left[i]; }
    }

    // HEVC 8.4.4.2.5：dcVal = (Σ p[x][-1] + Σ p[-1][y] + nTbS) >> (k+1)，k=log2(nTbS)。
    const int log2n = log2_of(n);
    int sum = 0;
    for (int i = 0; i < n; ++i) sum += t[i] + l[i];
    const int dc = (sum + n) >> (log2n + 1);

    // cIdx==0 && nTbS<32 时的 DC 首行/首列边界滤波。常数参考下它是恒等变换
    // （flat128 逐位一致），非平坦参考下必须与解码端一致，否则编码端重建
    // 与 ffmpeg 解码发散，并沿帧内预测参考链逐 CU 累积。
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            int v;
            if (x == 0 && y == 0)     v = (l[0] + 2 * dc + t[0] + 2) >> 2;
            else if (y == 0)          v = (t[x] + 3 * dc + 2) >> 2;
            else if (x == 0)          v = (l[y] + 3 * dc + 2) >> 2;
            else                       v = dc;
            dst[y * n + x] = static_cast<std::uint8_t>(v);
        }
    }
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
