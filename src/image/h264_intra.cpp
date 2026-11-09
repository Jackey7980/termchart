// h264_intra.cpp —— H.264 帧内宏块压缩档：4x4 整数变换 + 量化 + CAVLC
//
// 与 I_PCM 档互补：I_PCM 无损但一帧就是原始像素大小，这里走 I_4x4 帧内预测 +
// 整数变换/量化 + CAVLC，体积降一两个数量级。只做 baseline 的 I_4x4 宏块
// （mb_type=0），不碰帧间/去块滤波/CABAC；预测模式只用 V(0)/H(1)/DC(2)。
//
// 重建帧（rec）与解码端严格同步：预测只用 rec 样本，量化用标准 MF/levelScale。
// 码表见 h264_cavlc_tables.hpp。
#include "h264_intra.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "h264_bits.hpp"
#include "h264_cavlc_tables.hpp"

namespace termchart::image::h264_detail {
namespace {

// ── 量化表 ────────────────────────────────────────────────────────────────
// kMf 列序：0=偶偶 (0,0)(0,2)(2,0)(2,2)，1=混合，2=奇奇 (1,1)(1,3)(3,1)(3,3)。
// kLevelScale 与 kMf 同列同义，编码端量化与解码端反量化必须成对使用。
constexpr uint16_t kMf[6][3] = {
    {13107, 8066, 5243}, {11916, 7490, 4660}, {10082, 6554, 4194},
    {9362, 5825, 3647},  {8192, 5243, 3355},  {7282, 4559, 2893},
};
constexpr uint16_t kLevelScale[6][3] = {
    {10, 13, 16}, {11, 14, 18}, {13, 16, 20}, {14, 18, 23}, {16, 20, 25}, {18, 23, 29},
};

inline int xy_class(int i, int j) {
    const int a = i & 1;
    const int b = j & 1;
    if (a == 0 && b == 0) return 0;
    if (a == 1 && b == 1) return 2;
    return 1;
}

// ── 4x4 整数变换（H.264 正向核心变换，块内 idx = y*4 + x）───────────────
void fwd_transform(const int16_t* res, int16_t* out) {
    int32_t t[16];
    for (int i = 0; i < 4; ++i) {
        const int16_t* r = res + i * 4;
        const int32_t s0 = r[0] + r[3], s1 = r[1] + r[2];
        const int32_t s2 = r[1] - r[2], s3 = r[0] - r[3];
        t[i * 4 + 0] = s0 + s1;
        t[i * 4 + 1] = s2 + (s3 << 1);
        t[i * 4 + 2] = s0 - s1;
        t[i * 4 + 3] = s3 - (s2 << 1);
    }
    for (int j = 0; j < 4; ++j) {
        const int32_t d0 = t[j], d1 = t[4 + j], d2 = t[8 + j], d3 = t[12 + j];
        const int32_t s0 = d0 + d3, s1 = d1 + d2;
        const int32_t s2 = d1 - d2, s3 = d0 - d3;
        out[j] = static_cast<int16_t>(s0 + s1);
        out[4 + j] = static_cast<int16_t>(s2 + (s3 << 1));
        out[8 + j] = static_cast<int16_t>(s0 - s1);
        out[12 + j] = static_cast<int16_t>(s3 - (s2 << 1));
    }
}

// ── 4x4 逆整数变换（解码端同款），含最终 (x + 32) >> 6 舍入 ──────────────
void inv_transform(const int32_t* in, int16_t* out) {
    int32_t t[16];
    for (int i = 0; i < 4; ++i) {
        const int32_t d0 = in[i * 4 + 0], d1 = in[i * 4 + 1];
        const int32_t d2 = in[i * 4 + 2], d3 = in[i * 4 + 3];
        const int32_t e0 = d0 + d2, e1 = d0 - d2;
        const int32_t e2 = (d1 >> 1) - d3, e3 = d1 + (d3 >> 1);
        t[i * 4 + 0] = e0 + e3;
        t[i * 4 + 1] = e1 + e2;
        t[i * 4 + 2] = e1 - e2;
        t[i * 4 + 3] = e0 - e3;
    }
    for (int j = 0; j < 4; ++j) {
        const int32_t d0 = t[j], d1 = t[4 + j], d2 = t[8 + j], d3 = t[12 + j];
        const int32_t e0 = d0 + d2, e1 = d0 - d2;
        const int32_t e2 = (d1 >> 1) - d3, e3 = d1 + (d3 >> 1);
        out[j] = static_cast<int16_t>((e0 + e3 + 32) >> 6);
        out[4 + j] = static_cast<int16_t>((e1 + e2 + 32) >> 6);
        out[8 + j] = static_cast<int16_t>((e1 - e2 + 32) >> 6);
        out[12 + j] = static_cast<int16_t>((e0 - e3 + 32) >> 6);
    }
}

// ── 量化 / 反量化 ────────────────────────────────────────────────────────
// 返回非零系数个数。dc2x2=true 时用于色度 DC 2x2：正向哈达玛增益 4（4x4 的 16 是
// 1/4），逆向哈达玛 `(sum+1)>>1` 只回归 1/2，故 qbits 比亮度多 1 而不是少 2。
int quant_block(const int16_t* w, int16_t* z, int qp, bool dc2x2) {
    const int qbits = 15 + qp / 6 + (dc2x2 ? 1 : 0);
    const int f = (1 << qbits) / 3;
    const int mf_row = qp % 6;
    int nz = 0;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            const int k = i * 4 + j;
            const int mf = kMf[mf_row][dc2x2 ? 0 : xy_class(i, j)];
            const int a = std::abs(w[k]);
            const int v = (a * mf + f) >> qbits;
            z[k] = static_cast<int16_t>(w[k] < 0 ? -v : v);
            nz += (v != 0) ? 1 : 0;
        }
    }
    return nz;
}

void dequant_block(const int16_t* z, int32_t* w, int qp, bool dc2x2) {
    const int shift = qp / 6;
    const int ls_row = qp % 6;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            const int k = i * 4 + j;
            const int ls = kLevelScale[ls_row][dc2x2 ? 0 : xy_class(i, j)];
            w[k] = static_cast<int32_t>(z[k]) * ls * (1 << shift);
        }
    }
}

// 色度 DC 2x2 正向/逆向哈达玛（增益 4 已被 quant_block 的 qbits-2 吸收）。
void fwd_hadamard2x2(const int32_t* dc, int16_t* w) {
    const int32_t a = dc[0] + dc[1], b = dc[0] - dc[1];
    const int32_t c = dc[2] + dc[3], d = dc[2] - dc[3];
    w[0] = static_cast<int16_t>(a + c);
    w[1] = static_cast<int16_t>(b + d);
    w[2] = static_cast<int16_t>(a - c);
    w[3] = static_cast<int16_t>(b - d);
}

void inv_hadamard2x2(const int32_t* w, int32_t* dc) {
    const int32_t a = w[0] + w[2], b = w[1] + w[3];
    const int32_t c = w[0] - w[2], d = w[1] - w[3];
    // 2x2 逆变换的 1/2 归一化并入 >>1 舍入
    dc[0] = (a + b + 1) >> 1;
    dc[1] = (c + d + 1) >> 1;
    dc[2] = (a - b + 1) >> 1;
    dc[3] = (c - d + 1) >> 1;
}

// ── CAVLC 残差块写入（规范 9.2）─────────────────────────────────────────
// raster：光栅序量化电平；chroma_dc=true 时只有 4 个（2x2，扫描序即光栅序）。
// nC < 0 表示色度 DC 块，用表 9-5 的 4x5 专用 coeff_token。
int write_residual_block(BitWriter& w, const int16_t* raster, int n, int nC, bool chroma_dc,
                         bool already_scanned = false) {
#ifdef TERMCHART_H264_DEBUG
    const size_t bit0 = w.bit_size();
#endif
    int16_t lvl[16] = {};
    for (int i = 0; i < n; ++i)
        lvl[i] = already_scanned ? raster[i] : raster[chroma_dc ? i : kZigzag[i]];

    int tc = 0;
    int pos[16] = {};
    for (int i = 0; i < n; ++i) {
        if (lvl[i] != 0) pos[tc++] = i;
    }
    const int tab = chroma_dc ? -1 : kCoeffTokenTableIndex[std::min(std::max(nC, 0), 16)];
    if (tc == 0) {
        if (chroma_dc) {
            w.u(kChromaDcCoeffTokenBits[0], kChromaDcCoeffTokenLen[0]);
        } else {
            w.u(kCoeffTokenBits[tab][0], kCoeffTokenLen[tab][0]);
        }
#ifdef TERMCHART_H264_DEBUG
        std::fprintf(stderr, "RES %zu..%zu n=%d cd=%d nC=%d tc=0\n", bit0, w.bit_size(), n,
                     chroma_dc ? 1 : 0, nC);
#endif
        return 0;
    }

    const int last = pos[tc - 1];
    // TrailingOnes 只数「从最高频非零系数起连续的 ±1」，中间的 0 不参与，
    // 因此必须沿 pos[] 走，不能沿扫描位置倒着走。
    int te = 0;
    for (int k = tc - 1; k >= 0 && te < 3; --k) {
        if (std::abs(lvl[pos[k]]) == 1) {
            ++te;
        } else {
            break;
        }
    }
    const int ct = tc * 4 + te;
    if (chroma_dc) {
        w.u(kChromaDcCoeffTokenBits[ct], kChromaDcCoeffTokenLen[ct]);
    } else {
        w.u(kCoeffTokenBits[tab][ct], kCoeffTokenLen[tab][ct]);
    }
    for (int k = tc - 1; k >= tc - te; --k) w.u(lvl[pos[k]] < 0 ? 1u : 0u, 1);

    // level_prefix / level_suffix（suffixLength 自适应）
    int suffix = (tc > 10 && te < 3) ? 1 : 0;
    // pos[tc-1] 起往低频的前 te 个是 TrailingOnes（上面已写符号位）；
    // 真正的 level[i] (i=te..tc-1) 对应 pos[tc-1-i]，即从
    // k = tc-1-te 开始向低频遍历。写成 k=tc-1..te 会重编码 trailing ones
    // 并漏掉最低频（DC）系数。
    const int first = tc - 1 - te;
    for (int k = first; k >= 0; --k) {
        const int level = lvl[pos[k]];
        const int mag = std::abs(level);
        int code = (level > 0) ? ((mag << 1) - 2) : ((mag << 1) - 1);
        // 规范 9.2.2：TrailingOnes<3 时，首个被编码的 level（解码端
        // level[trailing_ones]）的 levelCode 要减 2，解码端对称加回。
        if (k == first && te < 3) code -= 2;
        // level_prefix / level_suffix（规范 9.2.2）。解码端对
        // prefix==14 && suffixLength==0 会额外读 4 位 suffix，对 prefix==15
        // 会读 12 位，两种 escape 都必须按同一偏置写，否则位流整体错位。
        int prefix, ssz, sval;
        if (suffix == 0 && code >= 14 && code < 30) {
            prefix = 14;
            ssz = 4;
            sval = code - 14;
        } else if ((code >> suffix) < 15) {
            prefix = code >> suffix;
            ssz = suffix;
            sval = code & ((1 << suffix) - 1);
        } else {
            prefix = 15;
            ssz = 12;
            sval = code - (15 << suffix) - (suffix == 0 ? 15 : 0);
        }
        for (int z = 0; z < prefix; ++z) w.put_bit(0);
        w.put_bit(1);
        if (ssz > 0) w.u(static_cast<unsigned>(sval), ssz);
        // 规范 9.2.2 的两个更新条件是独立的 if（不是 if/else if）：
        // suffixLength 先由 0 升到 1，随后仍要按本系数的幅值再判断一次。
        // 少做第二次判断会让第二个及之后的 level 用错 suffixLength。
        if (suffix == 0) suffix = 1;
        if (mag > (3 << (suffix - 1)) && suffix < 6) ++suffix;
    }

    const int total_zeros = last + 1 - tc;
    if (tc < n) {
        if (chroma_dc) {
            w.u(kChromaDcTotalZerosBits[tc - 1][total_zeros],
                kChromaDcTotalZerosLen[tc - 1][total_zeros]);
        } else {
            w.u(kTotalZerosBits[tc - 1][total_zeros], kTotalZerosLen[tc - 1][total_zeros]);
        }
    }

    int zeros_left = total_zeros;
    for (int k = tc - 1; k >= 1; --k) {
        if (zeros_left <= 0) break;
        const int run = pos[k] - pos[k - 1] - 1;
        const int row = std::min(zeros_left, 7) - 1;
        w.u(kRunBeforeBits[row][run], kRunBeforeLen[row][run]);
        zeros_left -= run;
    }
#ifdef TERMCHART_H264_DEBUG
    std::fprintf(stderr, "RES %zu..%zu n=%d cd=%d nC=%d tc=%d te=%d last=%d tz=%d lvl=",
                 bit0, w.bit_size(), n, chroma_dc ? 1 : 0, nC, tc, te, last, total_zeros);
    for (int k = 0; k < tc; ++k) std::fprintf(stderr, "%d,", lvl[pos[k]]);
    std::fprintf(stderr, "\n");
#endif
    return tc;
}

// ── 帧内预测：只用 V / H / DC 三种 ──────────────────────────────────────
// 内部模式常量与 luma 的 Intra4x4PredMode 编码一致（0=V, 1=H, 2=DC）；
// 色度 8x8 的 IntraChromaPredMode 编码是 0=DC, 1=H, 2=V，写出时需换位。
enum PredMode { kVert = 0, kHoriz = 1, kDc = 2 };

// a[] = 上方 n 个样本，b[] = 左侧 n 个样本；has_t/has_l 标明该侧是否真实可用。
// 规范 8.3.1.2.1 / 8.3.3：DC 预测只平均可用侧的样本，两侧都不可用时为 128。
// 解码端会把边界块的 DC 模式替换成 LEFT_DC_PRED / TOP_DC_PRED（单侧 DC），
// 因此这里不能把缺失侧当作 128 参与平均，否则重建与解码端不一致。
void pred_from_neigh(int16_t* pred, int n, int mode, const int* a, const int* b,
                     bool has_t, bool has_l) {
    if (mode == kVert) {
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) pred[y * n + x] = static_cast<int16_t>(a[x]);
        }
        return;
    }
    if (mode == kHoriz) {
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) pred[y * n + x] = static_cast<int16_t>(b[y]);
        }
        return;
    }
    if (n == 8) {
        // 规范 8.3.3 的 Intra_8x8_DC：按 4 个 4x4 区域分别取对应的上/左样本，
        // 不是整个 8x8 一起平均。top/left 不可用时分别退化为 left/top DC，
        // 两侧都不可用则 128（对应解码端 LEFT/TOP/DC_128_PRED8x8）。
        int a0 = 0, a1 = 0, b0 = 0, b1 = 0;
        for (int i = 0; i < 4; ++i) {
            a0 += a[i]; a1 += a[4 + i];
            b0 += b[i]; b1 += b[4 + i];
        }
        int q[2][2];
        if (has_t && has_l) {
            q[0][0] = (a0 + b0 + 4) >> 3;
            q[1][0] = (a1 + 2) >> 2;
            q[0][1] = (b1 + 2) >> 2;
            q[1][1] = (a1 + b1 + 4) >> 3;
        } else if (has_t) {
            q[0][0] = q[0][1] = (a0 + 2) >> 2;
            q[1][0] = q[1][1] = (a1 + 2) >> 2;
        } else if (has_l) {
            q[0][0] = q[1][0] = (b0 + 2) >> 2;
            q[0][1] = q[1][1] = (b1 + 2) >> 2;
        } else {
            q[0][0] = q[1][0] = q[0][1] = q[1][1] = 128;
        }
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                pred[y * 8 + x] = static_cast<int16_t>(q[x >> 2][y >> 2]);
        return;
    }
    int sum = 0, cnt = 0;
    if (has_t)
        for (int i = 0; i < n; ++i) { sum += a[i]; ++cnt; }
    if (has_l)
        for (int i = 0; i < n; ++i) { sum += b[i]; ++cnt; }
    const int dc = cnt ? (sum + cnt / 2) / cnt : 128;
    for (int i = 0; i < n * n; ++i) pred[i] = static_cast<int16_t>(dc);
}

// 预测残差的绝对差之和，用于选模式。
int sad_block(const uint8_t* src, int stride, int n, const int16_t* pred) {
    int sad = 0;
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            sad += std::abs(static_cast<int>(src[y * stride + x]) - pred[y * n + x]);
        }
    }
    return sad;
}

// ── 帧级编码状态 ────────────────────────────────────────────────────────
struct FrameEnc {
    const YuvFrame& src;
    int qp = 26;
    int mbs_x = 0, mbs_y = 0;
    int bw = 0, bh = 0;    // 亮度 4x4 块栅格
    int cbw = 0, cbh = 0;  // 色度 4x4 块栅格（每分量）
    std::vector<uint8_t> ry, ru, rv;          // 重建样本
    std::vector<uint8_t> mode_y;              // 亮度 4x4 预测模式图
    std::vector<uint8_t> tc_y, tc_u, tc_v;    // TotalCoeff 图（nC 用）

    FrameEnc(const YuvFrame& f, int q) : src(f), qp(q) {
        mbs_x = f.cw / 16;
        mbs_y = f.ch / 16;
        bw = mbs_x * 4;
        bh = mbs_y * 4;
        cbw = mbs_x * 2;
        cbh = mbs_y * 2;
        ry.assign(static_cast<size_t>(f.cw) * f.ch, 0);
        const int cs = f.cw / 2;
        const int ch2 = f.ch / 2;
        ru.assign(static_cast<size_t>(cs) * ch2, 0);
        rv.assign(static_cast<size_t>(cs) * ch2, 0);
        mode_y.assign(static_cast<size_t>(bw) * bh, 2);
        tc_y.assign(static_cast<size_t>(bw) * bh, 0);
        tc_u.assign(static_cast<size_t>(cbw) * cbh, 0);
        tc_v.assign(static_cast<size_t>(cbw) * cbh, 0);
    }
};

// 编一个亮度 4x4 块：选模式 → 变换/量化 → 重建。z_out 为光栅序量化电平。
int luma_block(FrameEnc& f, int bx, int by, int16_t* z_out) {
    const int rs = f.src.cw;
    const int px = bx * 4, py = by * 4;
    const bool has_l = bx > 0, has_t = by > 0;
    uint8_t* rec = f.ry.data();  // 重建目标，非只读
    int a[4], b[4];
    for (int i = 0; i < 4; ++i) {
        a[i] = has_t ? rec[(py - 1) * rs + px + i] : 128;
        b[i] = has_l ? rec[(py + i) * rs + px - 1] : 128;
    }
    const uint8_t* sp = f.src.y.data() + static_cast<size_t>(py) * rs + px;
    int16_t pred[16];
    int best = kDc;
    int best_sad = -1;
    for (int m = 0; m < 3; ++m) {
        if (m == kVert && !has_t) continue;
        if (m == kHoriz && !has_l) continue;
        pred_from_neigh(pred, 4, m, a, b, has_t, has_l);
        const int sad = sad_block(sp, rs, 4, pred);
        if (best_sad < 0 || sad < best_sad) {
            best_sad = sad;
            best = m;
        }
    }
    pred_from_neigh(pred, 4, best, a, b, has_t, has_l);

    int16_t res[16];
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            res[y * 4 + x] = static_cast<int16_t>(sp[y * rs + x] - pred[y * 4 + x]);
        }
    }
    int16_t w[16];
    fwd_transform(res, w);
    const int nz = quant_block(w, z_out, f.qp, false);

    int32_t wq[16];
    int16_t rr[16];
    dequant_block(z_out, wq, f.qp, false);
    inv_transform(wq, rr);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            rec[static_cast<size_t>(py + y) * rs + px + x] =
                static_cast<uint8_t>(clamp8(pred[y * 4 + x] + rr[y * 4 + x]));
        }
    }
    const size_t gi = static_cast<size_t>(by) * f.bw + bx;
    f.mode_y[gi] = static_cast<uint8_t>(best);
    f.tc_y[gi] = static_cast<uint8_t>(nz > 16 ? 16 : nz);
    return nz;
}

// ── 色度 DC 2x2 的量化/反量化（只有 4 个系数，不能套用 16 系数的函数）──
int quant_dc2x2(const int16_t* w, int16_t* z, int qp) {
    const int qbits = 16 + qp / 6;
    const int fr = (1 << qbits) / 3;
    const int mf = kMf[qp % 6][0];
    int nz = 0;
    for (int i = 0; i < 4; ++i) {
        const int v = (std::abs(w[i]) * mf + fr) >> qbits;
        z[i] = static_cast<int16_t>(w[i] < 0 ? -v : v);
        nz += (v != 0) ? 1 : 0;
    }
    return nz;
}

void dequant_dc2x2(const int16_t* z, int32_t* w, int qp) {
    const int ls = kLevelScale[qp % 6][0] * (1 << (qp / 6));
    for (int i = 0; i < 4; ++i) w[i] = static_cast<int32_t>(z[i]) * ls;
}

// ── 色度宏块（8x8 × 2 分量）─────────────────────────────────────────────
struct ChromaMb {
    int mode = kDc;                 // 内部模式，写码时换成 chroma 编码
    int16_t dc_lvl[2][4] = {};      // 2x2 DC 量化电平
    int16_t ac_lvl[2][4][16] = {};  // 4 个 4x4 AC 量化电平
    int tc_dc[2] = {};
    int tc_ac[2][4] = {};
};

void chroma_mb(FrameEnc& f, int mbx, int mby, ChromaMb& out) {
    const int cs = f.src.cw / 2;
    const int cx = mbx * 8, cy = mby * 8;
    const bool has_l = mbx > 0, has_t = mby > 0;
    uint8_t* rec[2] = {f.ru.data(), f.rv.data()};  // 重建目标，非只读
    const uint8_t* srcp[2] = {f.src.u.data(), f.src.v.data()};

    int a[8], b[8];
    int16_t pred[64];
    int best = kDc, best_sad = -1;
    for (int m = 0; m < 3; ++m) {
        if (m == kVert && !has_t) continue;
        if (m == kHoriz && !has_l) continue;
        int sad = 0;
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < 8; ++i) {
                a[i] = has_t ? rec[c][(cy - 1) * cs + cx + i] : 128;
                b[i] = has_l ? rec[c][(cy + i) * cs + cx - 1] : 128;
            }
            pred_from_neigh(pred, 8, m, a, b, has_t, has_l);
            sad += sad_block(srcp[c] + static_cast<size_t>(cy) * cs + cx, cs, 8, pred);
        }
        if (best_sad < 0 || sad < best_sad) {
            best_sad = sad;
            best = m;
        }
    }
    out.mode = best;

    for (int c = 0; c < 2; ++c) {
        for (int i = 0; i < 8; ++i) {
            a[i] = has_t ? rec[c][(cy - 1) * cs + cx + i] : 128;
            b[i] = has_l ? rec[c][(cy + i) * cs + cx - 1] : 128;
        }
        pred_from_neigh(pred, 8, best, a, b, has_t, has_l);
        const uint8_t* sp = srcp[c] + static_cast<size_t>(cy) * cs + cx;

        int32_t dcv[4];
        int16_t w4[4][16];
        for (int blk = 0; blk < 4; ++blk) {
            const int ox = (blk & 1) * 4, oy = (blk >> 1) * 4;
            int16_t res[16];
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    res[y * 4 + x] = static_cast<int16_t>(
                        sp[static_cast<size_t>(oy + y) * cs + ox + x] - pred[(oy + y) * 8 + ox + x]);
                }
            }
            fwd_transform(res, w4[blk]);
            dcv[blk] = w4[blk][0];
            w4[blk][0] = 0;  // DC 走 2x2 哈达玛
        }

        int16_t wd[4];
        fwd_hadamard2x2(dcv, wd);
        out.tc_dc[c] = quant_dc2x2(wd, out.dc_lvl[c], f.qp);

        int32_t dq[4], dcr[4];
        dequant_dc2x2(out.dc_lvl[c], dq, f.qp);
        inv_hadamard2x2(dq, dcr);

        for (int blk = 0; blk < 4; ++blk) {
            const int ox = (blk & 1) * 4, oy = (blk >> 1) * 4;
            out.tc_ac[c][blk] = quant_block(w4[blk], out.ac_lvl[c][blk], f.qp, false);
            int32_t wq[16];
            dequant_block(out.ac_lvl[c][blk], wq, f.qp, false);
            wq[0] = dcr[blk];
            int16_t rr[16];
            inv_transform(wq, rr);
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    rec[c][static_cast<size_t>(cy + oy + y) * cs + cx + ox + x] = static_cast<uint8_t>(
                        clamp8(pred[(oy + y) * 8 + ox + x] + rr[y * 4 + x]));
                }
            }
            std::vector<uint8_t>& tcg = (c == 0) ? f.tc_u : f.tc_v;
            const size_t gi =
                static_cast<size_t>(mby * 2 + (blk >> 1)) * static_cast<size_t>(f.cbw) + static_cast<size_t>(mbx * 2) + (blk & 1);
            tcg[gi] = static_cast<uint8_t>(out.tc_ac[c][blk]);
        }
    }
}

// ── coded_block_pattern 表 9-4：I slice 的 intra 分支 codeNum → pattern ───
// 注意 intra 与 inter 两张表完全不同：intra 的 codeNum 0 表示 cbp=47（所有块
// 都有残差），codeNum 3 才表示 cbp=0。用错表会让纯色宏块被解码端当成满残差。
// 与 FFmpeg ff_h264_golomb_to_intra4x4_cbp 一致。
struct CbpCodeTable {
    uint8_t code[48] = {};
    constexpr CbpCodeTable() {
        const uint8_t to_pattern[48] = {
            47, 31, 15, 0,  23, 27, 29, 30, 7,  11, 13, 14, 39, 43, 45, 46,
            16, 3,  5,  10, 12, 19, 21, 26, 28, 35, 37, 42, 44, 1,  2,  4,
            8,  17, 18, 20, 24, 6,  9,  22, 25, 32, 33, 34, 36, 40, 38, 41,
        };
        for (int i = 0; i < 48; ++i) code[to_pattern[i]] = static_cast<uint8_t>(i);
    }
};
constexpr CbpCodeTable kCbp;

// nC：取左、上相邻块的 TotalCoeff 平均（缺一侧用另一侧，都缺则 0）。
int nc_of(const std::vector<uint8_t>& tc, int stride, int bx, int by) {
    const bool has_l = bx > 0, has_t = by > 0;

    const size_t stride_t = static_cast<size_t>(stride); 
    const size_t bx_t = static_cast<size_t>(bx); 

    if (has_l && has_t) {
        const int a = tc[static_cast<size_t>(by) * stride_t + bx_t - 1];
        const int b = tc[static_cast<size_t>(by - 1) * stride_t + bx_t];
        return (a + b + 1) >> 1;
    }
    if (has_l) return tc[static_cast<size_t>(by) * stride_t + bx_t - 1];
    if (has_t) return tc[static_cast<size_t>(by - 1) * stride_t + bx_t];
    return 0;
}

// 内部模式 → IntraChromaPredMode 编码（0=DC, 1=H, 2=V）。
unsigned chroma_code(int mode) { return mode == kDc ? 0u : (mode == kHoriz ? 1u : 2u); }

std::string build_slice(FrameEnc& f, int idr_id) {
    BitWriter w;
    w.ue(0);     // first_mb_in_slice
    w.ue(2);     // slice_type = I（参考型，IDR 要求 nal_ref_idc != 0）
    w.ue(0);     // pic_parameter_set_id
    w.u(0, 4);   // frame_num
    w.ue(static_cast<unsigned>(idr_id));
    w.u(0, 4);   // pic_order_cnt_lsb
    w.u(0, 1);   // no_output_of_prior_pics_flag
    w.u(0, 1);   // long_term_reference_flag
    w.se(0);     // slice_qp_delta

    for (int mby = 0; mby < f.mbs_y; ++mby) {
        for (int mbx = 0; mbx < f.mbs_x; ++mbx) {
            int16_t zy[16][16];
            for (int blk = 0; blk < 16; ++blk) {
                const int bx = mbx * 4 + (blk & 3), by = mby * 4 + (blk >> 2);
                luma_block(f, bx, by, zy[blk]);
            }
            ChromaMb cm;
            chroma_mb(f, mbx, mby, cm);

            int cbpy = 0;
            for (int q = 0; q < 4; ++q) {
                const int bx0 = (q & 1) * 2, by0 = (q >> 1) * 2;
                for (int i = 0; i < 2 && !(cbpy & (1 << q)); ++i) {
                    for (int j = 0; j < 2; ++j) {
                        const size_t gi =
                            static_cast<size_t>(mby * 4 + by0 + i) * static_cast<size_t>(f.bw) + static_cast<size_t>(mbx * 4 + bx0 + j);
                        if (f.tc_y[gi] != 0) {
                            cbpy |= 1 << q;
                            break;
                        }
                    }
                }
            }
            // CodedBlockPatternChroma: 0=无残差, 1=仅 DC, 2=含 AC。
            bool any_ac = false;
            for (int c = 0; c < 2; ++c)
                for (int blk = 0; blk < 4; ++blk)
                    any_ac = any_ac || cm.tc_ac[c][blk] != 0;
            const bool any_dc = cm.tc_dc[0] != 0 || cm.tc_dc[1] != 0;
            const int cbp_c = any_ac ? 2 : (any_dc ? 1 : 0);
            const int pattern = cbpy | (cbp_c << 4);

            w.ue(0);  // mb_type = I_4x4
            // 16 个 prev/rem 必须按解码端的 4x4 块索引序（scan8，按 8x8 象限
            // 分组：0,1,2,3 → (0,0),(1,0),(0,1),(1,1)；4..7 → 右上 8x8）写，
            // 不能按 4x4 光栅 blk 序。mode_y/tc_y 仍以真实 (bx,by) 光栅存放。
            for (int i4 = 0; i4 < 16; ++i4) {
                const int lbx = (i4 & 1) + ((i4 >> 2) & 1) * 2;
                const int lby = ((i4 >> 1) & 1) + (i4 >> 3) * 2;
                const int bx = mbx * 4 + lbx, by = mby * 4 + lby;

                const size_t bw_t = static_cast<size_t>(f.bw);
                const size_t bx_t = static_cast<size_t>(bx);

                const int mode = f.mode_y[static_cast<size_t>(by) * bw_t + bx_t];
                // 规范 8.3.1.2.1：左/上任一邻居不可用（解码端 cache 中为 -1，
                // min<0）时 predIntra4x4PredMode 直接取 DC(2)，不能把缺失值
                // 当成 2 再参与 min，否则 pm 与解码端不同，rem 编码整体错位。
                const bool has_l = bx > 0, has_t = by > 0;
                const int pa =
                    has_l ? f.mode_y[static_cast<size_t>(by) * bw_t + bx_t - 1] : 2;
                const int pb =
                    has_t ? f.mode_y[static_cast<size_t>(by - 1) * bw_t + bx_t] : 2;
                const int pm = (has_l && has_t) ? std::min(pa, pb) : 2;
                // 规范 8.3.1.2：rem_intra4x4_pred_mode 的 3 bit 只覆盖「除 pm 外的
                // 8 个模式」，mode == pm 无法用 rem 表达，必须走 prev flag=1。
                if (mode == pm) {
                    w.u(1, 1);  // prev_intra4x4_pred_mode_flag = 1
                } else {
                    w.u(0, 1);
                    w.u(static_cast<unsigned>(mode < pm ? mode : mode - 1), 3);
                }
            }
            w.ue(chroma_code(cm.mode));
            w.ue(kCbp.code[pattern]);
            if (pattern != 0) w.se(0);  // mb_qp_delta
#ifdef TERMCHART_H264_DEBUG
            std::fprintf(stderr, "  MB(%d,%d) bit=%zu pattern=%d cbpy=%d cbpc=%d\n", mbx, mby,
                         w.bit_size(), pattern, cbpy, cbp_c);
#endif

            // 残差按「8x8 象限分组、象限内 4 个 4x4 块」的顺序写（规范 7.3.5 /
            // 解码端 decode_luma_residual）：q=0 是 blk 0,1,4,5，q=1 是 2,3,6,7…
            // 若按 blk 光栅递增写，解码端的 nC 预测值会对不上，coeff_token
            // 码长随之错位。重建（预测依赖左/上邻居）仍按光栅顺序，见上面的
            // luma_block 循环，两者互不影响。
            for (int q = 0; q < 4; ++q) {
                if (!(cbpy & (1 << q))) continue;
                for (int i4 = 0; i4 < 4; ++i4) {
                    const int blk =
                        (q >> 1) * 8 + (q & 1) * 2 + (i4 >> 1) * 4 + (i4 & 1);
                    const int bx = mbx * 4 + (blk & 3), by = mby * 4 + (blk >> 2);
                    write_residual_block(w, zy[blk], 16, nc_of(f.tc_y, f.bw, bx, by), false);
                }
            }
            if (cbp_c != 0) {
                write_residual_block(w, cm.dc_lvl[0], 4, 0, true);
                write_residual_block(w, cm.dc_lvl[1], 4, 0, true);
                if (cbp_c == 2) {
                    for (int c = 0; c < 2; ++c) {
                        for (int blk = 0; blk < 4; ++blk) {
                            const int gx = mbx * 2 + (blk & 1), gy = mby * 2 + (blk >> 1);
                            // 色度 DC 已单独编码，AC 块只有 15 个系数：解码端用
                            // scan+1 / max_coeff=15，跳过 zigzag[0] 的 DC 位置。
                            int16_t ac15[15];
                            for (int i = 0; i < 15; ++i)
                                ac15[i] = cm.ac_lvl[c][blk][kZigzag[i + 1]];
                            write_residual_block(w, ac15, 15,
                                                 nc_of(c ? f.tc_v : f.tc_u, f.cbw, gx, gy), false,
                                                 true);
                        }
                    }
                }
            }
        }
    }
    return w.finish();
}

}  // namespace

std::string build_idr_slice_intra(const YuvFrame& f, int idr_id, int qp) {
    if (f.cw < 16 || f.ch < 16) return std::string{};
    FrameEnc enc(f, qp);
    return build_slice(enc, idr_id);
}

}  // namespace termchart::image::h264_detail
