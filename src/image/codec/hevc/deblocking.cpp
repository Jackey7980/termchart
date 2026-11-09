#include "deblocking.hpp"

#include <cstddef>

namespace termchart::image::codec::hevc {
namespace {

// FFmpeg libavcodec/hevc/filter.c: betatable[52] / tctable[54]
constexpr std::uint8_t kBetaTable[52] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24,
    26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56,
    58, 60, 62, 64,
};

constexpr std::uint8_t kTcTable[54] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  3,
    3,  3,  3,  4,  4,  4,  5,  5,  6,  6,  7,  8,  9,  10, 11, 13,
    14, 16, 18, 20, 22, 24,
};

constexpr int kMaxQp = 51;
constexpr int kDefaultIntraTcOffset = 2;

constexpr int clip3(int lo, int hi, int v) { return v < lo ? lo : (v > hi ? hi : v); }
constexpr int clip_pixel(int v) { return clip3(0, 255, v); }
constexpr int abs_i(int v) { return v < 0 ? -v : v; }

// 一段 8 样本边界（沿边界方向 8 个样本 = 两段 4 样本，各用 tc[0] / tc[1]）的亮度滤波。
// xstride 是"跨边界"方向步长，ystride 是"沿边界"方向步长：
//   垂直边界：xstride = 1，ystride = stride
//   水平边界：xstride = stride，ystride = 1
// 对应 FFmpeg 的 hevc_v_loop_filter_luma / hevc_h_loop_filter_luma。
void filter_luma_edge(std::uint8_t* pix, std::ptrdiff_t xstride, std::ptrdiff_t ystride,
                      int beta, int tc) {
    if (tc <= 0) return;  // tc == 0 时强弱滤波都不可能改动样本

    for (int j = 0; j < 2; ++j) {
        std::uint8_t* p = pix + j * 4 * ystride;
        const auto at = [p, xstride, ystride](int k, int along) {
            return static_cast<int>(p[k * xstride + along * ystride]);
        };
        const int p3 = at(-4, 0), p2 = at(-3, 0), p1 = at(-2, 0), p0 = at(-1, 0);
        const int q0 = at(0, 0), q1 = at(1, 0), q2 = at(2, 0), q3 = at(3, 0);
        const int tp3 = at(-4, 3), tp2 = at(-3, 3), tp1 = at(-2, 3), tp0 = at(-1, 3);
        const int tq0 = at(0, 3), tq1 = at(1, 3), tq2 = at(2, 3), tq3 = at(3, 3);

        // 判据只在沿边界方向的第一行与第四行上采样（line zero / line three）
        const int dp0 = abs_i(p2 - 2 * p1 + p0);
        const int dq0 = abs_i(q2 - 2 * q1 + q0);
        const int dp3 = abs_i(tp2 - 2 * tp1 + tp0);
        const int dq3 = abs_i(tq2 - 2 * tq1 + tq0);
        const int d0 = dp0 + dq0;
        const int d3 = dp3 + dq3;
        if (d0 + d3 >= beta) continue;

        const int beta_3 = beta >> 3;
        const int beta_2 = beta >> 2;
        const int tc25 = (tc * 5 + 1) >> 1;
        const bool strong = abs_i(p3 - p0) + abs_i(q3 - q0) < beta_3 &&
                            abs_i(p0 - q0) < tc25 &&
                            abs_i(tp3 - tp0) + abs_i(tq3 - tq0) < beta_3 &&
                            abs_i(tp0 - tq0) < tc25 &&
                            (d0 << 1) < beta_2 && (d3 << 1) < beta_2;

        if (strong) {
            // FFmpeg 调用点传的是 (tc2, tc2, tc2)（tc2 = tc << 1），即三个 clip 界都
            // 是 ±2*tc。
            const int t = tc << 1;
            for (int d = 0; d < 4; ++d) {
                std::uint8_t* s = p + d * ystride;
                const int sp3 = static_cast<int>(s[-4 * xstride]);
                const int sp2 = static_cast<int>(s[-3 * xstride]);
                const int sp1 = static_cast<int>(s[-2 * xstride]);
                const int sp0 = static_cast<int>(s[-1 * xstride]);
                const int sq0 = static_cast<int>(s[0]);
                const int sq1 = static_cast<int>(s[1 * xstride]);
                const int sq2 = static_cast<int>(s[2 * xstride]);
                const int sq3 = static_cast<int>(s[3 * xstride]);
                s[-1 * xstride] = static_cast<std::uint8_t>(clip_pixel(
                    sp0 + clip3(-t, t, ((sp2 + 2 * sp1 + 2 * sp0 + 2 * sq0 + sq1 + 4) >> 3) - sp0)));
                s[-2 * xstride] = static_cast<std::uint8_t>(clip_pixel(
                    sp1 + clip3(-t, t, ((sp2 + sp1 + sp0 + sq0 + 2) >> 2) - sp1)));
                s[-3 * xstride] = static_cast<std::uint8_t>(clip_pixel(
                    sp2 + clip3(-t, t, ((2 * sp3 + 3 * sp2 + sp1 + sp0 + sq0 + 4) >> 3) - sp2)));
                s[0] = static_cast<std::uint8_t>(clip_pixel(
                    sq0 + clip3(-t, t, ((sp1 + 2 * sp0 + 2 * sq0 + 2 * sq1 + sq2 + 4) >> 3) - sq0)));
                s[1 * xstride] = static_cast<std::uint8_t>(clip_pixel(
                    sq1 + clip3(-t, t, ((sp0 + sq0 + sq1 + sq2 + 2) >> 2) - sq1)));
                s[2 * xstride] = static_cast<std::uint8_t>(clip_pixel(
                    sq2 + clip3(-t, t, ((2 * sq3 + 3 * sq2 + sq1 + sq0 + sp0 + 4) >> 3) - sq2)));
            }
        } else {
            const int nd_p = (dp0 + dp3 < ((beta + (beta >> 1)) >> 3)) ? 2 : 1;
            const int nd_q = (dq0 + dq3 < ((beta + (beta >> 1)) >> 3)) ? 2 : 1;
            const int tc_2 = tc >> 1;
            for (int d = 0; d < 4; ++d) {
                std::uint8_t* s = p + d * ystride;
                const int sp2 = static_cast<int>(s[-3 * xstride]);
                const int sp1 = static_cast<int>(s[-2 * xstride]);
                const int sp0 = static_cast<int>(s[-1 * xstride]);
                const int sq0 = static_cast<int>(s[0]);
                const int sq1 = static_cast<int>(s[1 * xstride]);
                const int sq2 = static_cast<int>(s[2 * xstride]);
                int delta0 = (9 * (sq0 - sp0) - 3 * (sq1 - sp1) + 8) >> 4;
                if (abs_i(delta0) >= 10 * tc) continue;
                delta0 = clip3(-tc, tc, delta0);
                s[-1 * xstride] = static_cast<std::uint8_t>(clip_pixel(sp0 + delta0));
                s[0] = static_cast<std::uint8_t>(clip_pixel(sq0 - delta0));
                if (nd_p > 1) {
                    const int deltap1 =
                        clip3(-tc_2, tc_2, (((sp2 + sp0 + 1) >> 1) - sp1 + delta0) >> 1);
                    s[-2 * xstride] = static_cast<std::uint8_t>(clip_pixel(sp1 + deltap1));
                }
                if (nd_q > 1) {
                    const int deltaq1 =
                        clip3(-tc_2, tc_2, (((sq2 + sq0 + 1) >> 1) - sq1 - delta0) >> 1);
                    s[1 * xstride] = static_cast<std::uint8_t>(clip_pixel(sq1 + deltaq1));
                }
            }
        }
    }
}

}  // namespace

int deblock_beta(int qp, int beta_offset_div2) {
    // FFmpeg: betatable[av_clip(qp + beta_offset, 0, MAX_QP)]，beta_offset 为 div2 的两倍
    return kBetaTable[clip3(0, kMaxQp, qp + 2 * beta_offset_div2)];
}

int deblock_tc(int qp, int boundary_strength, int tc_offset_div2) {
    // FFmpeg TC_CALC: tctable[av_clip(qp + DEFAULT_INTRA_TC_OFFSET*(bs-1) + tc_offset, 0, 53)]
    return kTcTable[clip3(0, kMaxQp + kDefaultIntraTcOffset,
                          qp + kDefaultIntraTcOffset * (boundary_strength - 1) +
                              2 * tc_offset_div2)];
}

void deblock_frame_luma(std::uint8_t* y, int width, int height, int stride,
                        const DeblockConfig& cfg) {
    if (y == nullptr || width <= 0 || height <= 0) return;
    if ((width % 8) != 0 || (height % 8) != 0) return;  // 编码器按 CTB 对齐，正常不会走到

    const int beta = deblock_beta(cfg.slice_qp, cfg.beta_offset_div2);
    const int tc = deblock_tc(cfg.slice_qp, 2, cfg.tc_offset_div2);
    if (tc <= 0) return;  // 低 QP 段 tctable 为 0：整帧不产生任何改动

    constexpr int kCtbSize = 64;
    for (int y0 = 0; y0 < height; y0 += kCtbSize) {
        const int y_end = (y0 + kCtbSize < height) ? y0 + kCtbSize : height;
        for (int x0 = 0; x0 < width; x0 += kCtbSize) {
            const int x_end = (x0 + kCtbSize < width) ? x0 + kCtbSize : width;
            // 与 FFmpeg 一致：非 CTB 行尾时最后一列 8 样本段推迟到右侧 CTB 处理
            // （那里以 x0-8 起始时用 left offset；本编码器 offset 恒 0，只影响顺序）
            const int x_end2 = (x_end != width) ? x_end - 8 : x_end;

            for (int yy = y0; yy < y_end; yy += 8) {
                // 垂直边界：左边界（x == 0）永不滤波
                for (int xx = (x0 > 0 ? x0 : 8); xx < x_end; xx += 8)
                    filter_luma_edge(y + static_cast<std::ptrdiff_t>(yy) * stride + xx, 1, stride,
                                     beta, tc);
                if (yy == 0) continue;  // 上边界（y == 0）永不滤波
                for (int xx = (x0 > 0 ? x0 - 8 : 0); xx < x_end2; xx += 8)
                    filter_luma_edge(y + static_cast<std::ptrdiff_t>(yy) * stride + xx, stride, 1,
                                     beta, tc);
            }
        }
    }
}

}  // namespace termchart::image::codec::hevc
