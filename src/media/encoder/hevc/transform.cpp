// transform.cpp —— HEVC 整数变换（DCT-II 4/8/16/32 + DST-VII 4x4）
//
// 与 ffmpeg libavcodec/hevc/dsp_template.c 的 idct_4/8/16/32 同构以保证
// 「编码端重建 == ffmpeg 解码」逐比特一致：
//   - 8/16/32 走 32 点基函数表（transform_table.hpp）按 step=32/N 抽样得到的 N×N 矩阵
//     做两次 1D 矩阵乘：第一级按列（shift=7）、第二级按行（shift=12），每级 clip_int16。
//     这等价于 ffmpeg 的 TR_N 蝶形（蝶形只是该矩阵乘的分解），数学结果逐比特一致。
//   - 4x4 DCT 保留逐常量蝶形（与矩阵法等价，矩阵法的 4x4 切片恰为 x265 g_t4）。
//   - 4x4 DST-VII 仅用于亮度帧内，保留 TR_4x4_LUMA 蝶形。
//
// fwd_* 用同一组矩阵做 adjoint（两段移位 x265 dct4/8/16/32：1/8、2/9、3/10、4/11），
// 与 inv_* 构成整数往返。纯整数（int64 累加，避免 32x32 溢出），无 HEVC 语法。
// 行优先 idx = y*N + x。
#include "transform.hpp"
#include "transform_table.hpp"

#include <algorithm>
#include <cstdint>

namespace termchart::image::codec::hevc {
namespace {

constexpr int kInvShift1 = 7;    // 逆变换第一级移位（列）
constexpr int kInvShift2 = 12;   // 逆变换第二级移位（行）

inline std::int16_t clip_int16(std::int64_t x) {
    return static_cast<std::int16_t>(std::clamp<std::int64_t>(x, -32768, 32767));
}

// ── 8/16/32 的 1D DCT 核 G（从 ffmpeg 蝶形精确导出，避免错误抽样）──────────
// ffmpeg 的 TR_8/16/32 蝶形等价于一次 1D 矩阵乘 G（偶数部递归到 TR_4）。
// 直接把蝶形按"无移位"执行于基向量，即可取出真正的变换核 G[k][n]。
static void idct1d_core(std::int32_t* x, int n) {
    if (n == 4) {
        const int e0 = 64 * x[0] + 64 * x[2];
        const int e1 = 64 * x[0] - 64 * x[2];
        const int o0 = 83 * x[1] + 36 * x[3];
        const int o1 = 36 * x[1] - 83 * x[3];
        const int t0 = e0 + o0, t1 = e1 + o1, t2 = e1 - o1, t3 = e0 - o0;
        x[0] = t0; x[1] = t1; x[2] = t2; x[3] = t3;
        return;
    }
    const int half = n / 2;
    const int stride = 32 / n;  // ffmpeg：8→4(transform[4j])、16→2(transform[2j])、32→1(transform[j])
    std::int32_t o[16] = {0};
    for (int i = 0; i < half; ++i)
        for (int j = 1; j < n; j += 2)
            o[i] += static_cast<std::int32_t>(kTr32[stride * j][i]) * x[j];
    std::int32_t evenx[32];
    for (int i = 0; i < half; ++i) evenx[i] = x[2 * i];
    idct1d_core(evenx, half);
    for (int i = 0; i < half; ++i) {
        x[i] = evenx[i] + o[i];
        x[n - 1 - i] = evenx[i] - o[i];
    }
}

// Gtab[ls-3][k][n]：size = 1<<ls（ls∈{3,4,5}）的 1D DCT 核。首次使用时构建。
inline int g_at(int log2_size, int k, int n) {
    const int sz = 1 << log2_size;
    static std::int16_t Gtab[3][32][32];
    static bool built = false;
    if (!built) {
        for (int ls = 3; ls <= 5; ++ls) {
            const int m = 1 << ls;
            const std::int32_t S = 1 << 20;
            for (int col = 0; col < m; ++col) {
                std::int32_t x[32] = {0};
                x[col] = S;
                idct1d_core(x, m);
                for (int kk = 0; kk < m; ++kk)
                    Gtab[ls - 3][kk][col] = static_cast<std::int16_t>(x[kk] / S);
            }
        }
        built = true;
    }
    (void)sz;
    return static_cast<int>(Gtab[log2_size - 3][k][n]);
}

// ── 4x4 DCT-II 正向核（x265 g_t4），与 ffmpeg idct 互逆 ──────────────────
constexpr std::int16_t kDct[4][4] = {
    {64,  64,  64,  64},
    {83,  36, -36, -83},
    {64, -64, -64,  64},
    {36, -83,  83, -36},
};

// ── 4x4 DST-VII 正向核（HEVC 表 8-3，按 128 缩放）────────────────────────
constexpr std::int16_t kDst[4][4] = {
    {29,  55,  74,  84},
    {74,  74,   0, -74},
    {84, -29, -74,  55},
    {55, -84,  74, -29},
};

// ffmpeg TR_4（DCT-II 逆）：先列(step=4,shift=7) 后行(step=1,shift=12)。
void inv_dct1_cols(std::int32_t* c) {
    for (int i = 0; i < 4; ++i) {
        const std::int32_t s0 = 64 * c[i + 0] + 64 * c[i + 8];
        const std::int32_t s1 = 64 * c[i + 0] - 64 * c[i + 8];
        const std::int32_t o0 = 83 * c[i + 4] + 36 * c[i + 12];
        const std::int32_t o1 = 36 * c[i + 4] - 83 * c[i + 12];
        c[i + 0]  = clip_int16((s0 + o0 + (1 << (kInvShift1 - 1))) >> kInvShift1);
        c[i + 4]  = clip_int16((s1 + o1 + (1 << (kInvShift1 - 1))) >> kInvShift1);
        c[i + 8]  = clip_int16((s1 - o1 + (1 << (kInvShift1 - 1))) >> kInvShift1);
        c[i + 12] = clip_int16((s0 - o0 + (1 << (kInvShift1 - 1))) >> kInvShift1);
    }
}
void inv_dct1_rows(std::int32_t* c) {
    for (int i = 0; i < 4; ++i) {
        const std::int32_t s0 = 64 * c[i * 4 + 0] + 64 * c[i * 4 + 2];
        const std::int32_t s1 = 64 * c[i * 4 + 0] - 64 * c[i * 4 + 2];
        const std::int32_t o0 = 83 * c[i * 4 + 1] + 36 * c[i * 4 + 3];
        const std::int32_t o1 = 36 * c[i * 4 + 1] - 83 * c[i * 4 + 3];
        c[i * 4 + 0] = clip_int16((s0 + o0 + (1 << (kInvShift2 - 1))) >> kInvShift2);
        c[i * 4 + 1] = clip_int16((s1 + o1 + (1 << (kInvShift2 - 1))) >> kInvShift2);
        c[i * 4 + 2] = clip_int16((s1 - o1 + (1 << (kInvShift2 - 1))) >> kInvShift2);
        c[i * 4 + 3] = clip_int16((s0 - o0 + (1 << (kInvShift2 - 1))) >> kInvShift2);
    }
}

// ffmpeg TR_4x4_LUMA（DST-VII 逆）。
void inv_dst1_cols(std::int32_t* c) {
    for (int i = 0; i < 4; ++i) {
        const std::int32_t c0 = c[i + 0] + c[i + 8];
        const std::int32_t c1 = c[i + 8] + c[i + 12];
        const std::int32_t c2 = c[i + 0] - c[i + 12];
        const std::int32_t c3 = 74 * c[i + 4];
        const std::int32_t a2 = 74 * (c[i + 0] - c[i + 8] + c[i + 12]);
        const std::int32_t a0 = 29 * c0 + 55 * c1 + c3;
        const std::int32_t a1 = 55 * c2 - 29 * c1 + c3;
        const std::int32_t a3 = 55 * c0 + 29 * c2 - c3;
        c[i + 8]  = clip_int16((a2 + (1 << (kInvShift1 - 1))) >> kInvShift1);
        c[i + 0]  = clip_int16((a0 + (1 << (kInvShift1 - 1))) >> kInvShift1);
        c[i + 4]  = clip_int16((a1 + (1 << (kInvShift1 - 1))) >> kInvShift1);
        c[i + 12] = clip_int16((a3 + (1 << (kInvShift1 - 1))) >> kInvShift1);
    }
}
void inv_dst1_rows(std::int32_t* c) {
    for (int i = 0; i < 4; ++i) {
        const std::int32_t c0 = c[i * 4 + 0] + c[i * 4 + 2];
        const std::int32_t c1 = c[i * 4 + 2] + c[i * 4 + 3];
        const std::int32_t c2 = c[i * 4 + 0] - c[i * 4 + 3];
        const std::int32_t c3 = 74 * c[i * 4 + 1];
        const std::int32_t a2 = 74 * (c[i * 4 + 0] - c[i * 4 + 2] + c[i * 4 + 3]);
        const std::int32_t a0 = 29 * c0 + 55 * c1 + c3;
        const std::int32_t a1 = 55 * c2 - 29 * c1 + c3;
        const std::int32_t a3 = 55 * c0 + 29 * c2 - c3;
        c[i * 4 + 2] = clip_int16((a2 + (1 << (kInvShift2 - 1))) >> kInvShift2);
        c[i * 4 + 0] = clip_int16((a0 + (1 << (kInvShift2 - 1))) >> kInvShift2);
        c[i * 4 + 1] = clip_int16((a1 + (1 << (kInvShift2 - 1))) >> kInvShift2);
        c[i * 4 + 3] = clip_int16((a3 + (1 << (kInvShift2 - 1))) >> kInvShift2);
    }
}

// ── 8/16/32 正向（adjoint）：tmp = G·r（列,shift s1）；coeff = tmp·Gᵀ（行,shift s2）──
void fwd_transform_large(const std::int16_t* residual, std::int32_t* coeff,
                         int log2_size) {
    const int n = 1 << log2_size;
    const int s1 = log2_size - 1;
    const int s2 = log2_size + 6;      // x265 dct8/16/32：2/9、3/10、4/11
    const std::int64_t add1 = 1 << (s1 - 1);
    const std::int64_t add2 = 1 << (s2 - 1);
    std::int32_t tmp[32 * 32];
    // 正变换是逆变换的转置（与 4x4 kDct=Gᵀ 同构）：coeff = Gᵀ·r·G。
    // 逆变换为 G·c·Gᵀ（ffmpeg idct），二者组合 = (G·Gᵀ)²·R，且 G·Gᵀ = 2^(ls+12)·I，
    // 总移位 (2ls+5)+(7+12)=2ls+24 恰好抵消 → 无量化往返 factor=1.0。
    // pass1：列变换 Y = Gᵀ·r
    for (int x = 0; x < n; ++x) {
        for (int k = 0; k < n; ++k) {
            std::int64_t s = 0;
            for (int i = 0; i < n; ++i)
                s += static_cast<std::int64_t>(g_at(log2_size, i, k)) * residual[i * n + x];
            tmp[k * n + x] = static_cast<std::int32_t>((s + add1) >> s1);
        }
    }
    // pass2：行变换 coeff = Y·G
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            std::int64_t s = 0;
            for (int k = 0; k < n; ++k)
                s += static_cast<std::int64_t>(tmp[y * n + k]) * g_at(log2_size, k, x);
            coeff[y * n + x] = static_cast<std::int32_t>((s + add2) >> s2);
        }
    }
}

// ── 8/16/32 逆（= ffmpeg idct_8/16/32）：第一级按列(shift7)，第二级按行(shift12) ──
// col_limit：解码端由最后一个非零系数推出的截断上限；编码端恒传 n（不截断）。
void inv_transform_large(const std::int32_t* coeff, std::int16_t* residual,
                         int log2_size, int col_limit) {
    const int n = 1 << log2_size;
    const int cl = std::min(col_limit, n);
    std::int32_t buf[32 * 32];
    // 逆变换须与 ffmpeg idct / 4x4 inv_dct1_cols 同构：整体为 G·C·Gᵀ（左乘 G、右乘 Gᵀ）。
    // pass1：列变换 Y = G·C（累加前 cl 行的系数，对应 ffmpeg 的 col_limit 截断）。
    // 关键：pass1 必须读原始 coeff、写独立 buf，绝不能 in-place 读写同一 buf。
    // 否则 k 递增时后续迭代会读到已被前一次覆盖的同列值，污染沿行方向累积，
    // 使 fwd→inv 无量化往返增益逐行衰减（size8/16/32 曾只剩 0.5x / 0.36x / 0.44x）。
    // 4x4 蝶形之所以一直正常，是因为它先把整列读入局部量再写，天然无别名。
    for (int x = 0; x < n; ++x) {
        for (int k = 0; k < n; ++k) {
            std::int64_t s = 0;
            for (int i = 0; i < cl; ++i)
                s += static_cast<std::int64_t>(g_at(log2_size, k, i)) * coeff[i * n + x];
            buf[k * n + x] = clip_int16((s + (1 << (kInvShift1 - 1))) >> kInvShift1);
        }
    }
    // pass2：行变换 R = Y·Gᵀ（右乘 Gᵀ；与正变换 Gᵀ·r·G 对称，整体 G·C·Gᵀ）。
    const int rl = std::min(cl, n);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            std::int64_t s = 0;
            for (int k = 0; k < rl; ++k)
                s += static_cast<std::int64_t>(g_at(log2_size, x, k)) * buf[y * n + k];
            residual[y * n + x] = clip_int16((s + (1 << (kInvShift2 - 1))) >> kInvShift2);
        }
    }
}

}  // namespace

void fwd_transform(const std::int16_t* residual, std::int32_t* coeff, int log2_size,
                   bool use_dst) {
    if (log2_size == 2 && use_dst) {
        // 4x4 DST-VII 正向：coeff = M·r·Mᵀ（shift 1 / 8）。
        std::int32_t tmp[16];
        const std::int64_t add1 = 1 << 0;
        for (int x = 0; x < 4; ++x)
            for (int i = 0; i < 4; ++i) {
                std::int64_t s = 0;
                for (int j = 0; j < 4; ++j) s += static_cast<std::int64_t>(kDst[i][j]) * residual[j * 4 + x];
                tmp[i * 4 + x] = static_cast<std::int32_t>((s + add1) >> 1);
            }
        const std::int64_t add2 = 1 << 7;
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) {
                std::int64_t s = 0;
                for (int k = 0; k < 4; ++k) s += static_cast<std::int64_t>(kDst[x][k]) * tmp[y * 4 + k];
                coeff[y * 4 + x] = static_cast<std::int32_t>((s + add2) >> 8);
            }
        return;
    }
    if (log2_size == 2) {
        // 4x4 DCT-II 正向（与矩阵法等价）。
        std::int32_t tmp[16];
        const std::int64_t add1 = 1 << 0;
        for (int x = 0; x < 4; ++x)
            for (int i = 0; i < 4; ++i) {
                std::int64_t s = 0;
                for (int j = 0; j < 4; ++j) s += static_cast<std::int64_t>(kDct[i][j]) * residual[j * 4 + x];
                tmp[i * 4 + x] = static_cast<std::int32_t>((s + add1) >> 1);
            }
        const std::int64_t add2 = 1 << 7;
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) {
                std::int64_t s = 0;
                for (int k = 0; k < 4; ++k) s += static_cast<std::int64_t>(kDct[x][k]) * tmp[y * 4 + k];
                coeff[y * 4 + x] = static_cast<std::int32_t>((s + add2) >> 8);
            }
        return;
    }
    fwd_transform_large(residual, coeff, log2_size);
}

void inv_transform(const std::int32_t* coeff, std::int16_t* residual, int log2_size,
                   bool use_dst, int col_limit) {
    if (log2_size == 2) {
        std::int32_t c[16];
        for (int i = 0; i < 16; ++i) c[i] = coeff[i];
        if (use_dst) {
            inv_dst1_cols(c);
            inv_dst1_rows(c);
        } else {
            inv_dct1_cols(c);
            inv_dct1_rows(c);
        }
        for (int i = 0; i < 16; ++i) residual[i] = static_cast<std::int16_t>(c[i]);
        return;
    }
    inv_transform_large(coeff, residual, log2_size, col_limit);
}

int inverse_col_limit(int last_x, int last_y, int log2_size) {
    const int n = 1 << log2_size;
    const int last = std::max(last_x, last_y);
    int col_limit = last_x + last_y + 4;
    if (last < 4) {
        col_limit = std::min(4, col_limit);
    } else if (last < 8) {
        col_limit = std::min(8, col_limit);
    } else if (last < 12) {
        col_limit = std::min(24, col_limit);
    }
    return std::min(col_limit, n);
}

void fwd_transform_4x4(const std::int16_t* residual, std::int32_t* coeff, bool use_dst) {
    fwd_transform(residual, coeff, 2, use_dst);
}

void inv_transform_4x4(const std::int32_t* coeff, std::int16_t* residual, bool use_dst) {
    inv_transform(coeff, residual, 2, use_dst, 4);
}

}  // namespace termchart::image::codec::hevc
