// residual_coding.cpp —— 4x4 TU 残差语法（按 FFmpeg ff_hevc_hls_residual_coding 翻译）
#include "residual_coding.hpp"

#include <cstdlib>
#include <utility>

#include "cabac_context.hpp"

namespace termchart::image::codec::hevc {

const std::uint8_t kDiagScan4x4X[16] = {0, 0, 1, 0, 1, 2, 0, 1, 2, 3, 1, 2, 3, 2, 3, 3};
const std::uint8_t kDiagScan4x4Y[16] = {0, 1, 0, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 3, 2, 3};
const std::uint8_t kDiagScan4x4Inv[4][4] = {
    {0, 2, 5, 9}, {1, 4, 8, 12}, {3, 7, 11, 14}, {6, 10, 13, 15},
};
const std::uint8_t kSigCoeffCtxMap4x4[16] = {
    0, 2, 1, 6, 3, 4, 7, 6, 4, 5, 7, 8, 5, 8, 8, 8,
};

// SCAN_HORIZ / SCAN_VERT 的 4x4 表逐项取自 FFmpeg cabac.c:346-358 与 :1228-1267。
namespace {
const std::uint8_t kHorizScanX[16] = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};
const std::uint8_t kHorizScanY[16] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3};
const std::uint8_t kVertScanX[16] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3};
const std::uint8_t kVertScanY[16] = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};
const std::uint8_t kHorizScanInv[4][4] = {
    {0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 10, 11}, {12, 13, 14, 15},
};
const std::uint8_t kVertScanInv[4][4] = {
    {0, 4, 8, 12}, {1, 5, 9, 13}, {2, 6, 10, 14}, {3, 7, 11, 15},
};
// FFmpeg cabac.c:1339-1359 的 ctx_idx_map[scan_idx][0..15]（log2_trafo_size==2 行）。
const std::uint8_t kSigCtxHoriz[16] = {0, 1, 4, 5, 2, 3, 4, 5, 6, 6, 8, 8, 7, 7, 8, 8};
const std::uint8_t kSigCtxVert[16] = {0, 2, 6, 7, 1, 3, 6, 7, 4, 4, 8, 8, 5, 5, 8, 8};

const Scan4x4Tables kTables[3] = {
    {kDiagScan4x4X, kDiagScan4x4Y, kSigCoeffCtxMap4x4, kDiagScan4x4Inv, false},
    {kHorizScanX, kHorizScanY, kSigCtxHoriz, kHorizScanInv, false},
    {kVertScanX, kVertScanY, kSigCtxVert, kVertScanInv, true},
};
}  // namespace

ScanIdx scan_idx_for_intra_mode(int mode) {
    if (mode >= 6 && mode <= 14) return kScanVert;
    if (mode >= 22 && mode <= 30) return kScanHoriz;
    return kScanDiag;
}

const Scan4x4Tables& scan4x4_tables(ScanIdx idx) {
    return kTables[idx == kScanHoriz ? 1 : (idx == kScanVert ? 2 : 0)];
}

namespace {

inline int abs_level(int v) { return v < 0 ? -v : v; }

struct Reader {
    const Bin* b;
    std::size_t n, p = 0;
    bool next(int& ctx, int& val) {
        if (p >= n) return false;
        ctx = b[p].ctx;
        val = b[p].val;
        ++p;
        return true;
    }
    bool decision(int expect_ctx, int& val) {
        int ctx;
        return next(ctx, val) && ctx == expect_ctx;
    }
    bool bypass(int& val) {
        int ctx;
        return next(ctx, val) && ctx == kBypassCtx;
    }
};

// FFmpeg decode_residual 的 greater1_ctx 演化：
//   greater1_ctx = (greater1_ctx + (greater1_ctx - 1U < 2)) & (flag - 1);
// 逐项展开：(g-1U < 2) 只在 g==1 或 g==2 时为 1，故只在这两处递增；
// flag==1 时 &(1-1)=&0 清零，flag==0 时 &0xFFFFFFFF 保持。
// 注意 g==0 时【不递增】（0-1U 回绕成极大值），早期实现写成 g+1 会让
// 「cc; 一个 level>1 的系数把 g 归零」之后紧接着的 level==1 系数用错上下文，
// 解码端算术解码器从此跑偏（多非零系数的块整块崩坏，单系数块却完全正常）。
inline int next_greater1_ctx(int g, int flag) {
    if (flag) return 0;
    if (g == 1 || g == 2) return g + 1;
    return g;
}

// 是否写 coeff_abs_level_remaining，以及其基准 level。
void level_base(int m, int first_gt1, const int* gt1, int gt2, int nb,
                int& tcl, bool& write_remaining) {
    if (m < 8 && m < nb) {
        tcl = 1 + gt1[m] + ((m == first_gt1) ? gt2 : 0);
        write_remaining = (tcl == ((m == first_gt1) ? 3 : 2));
    } else {
        tcl = 1;
        write_remaining = true;
    }
}

}  // namespace

std::vector<Bin> encode_residual_4x4(const std::int16_t* coeff, bool* ok, ScanIdx scan_idx) {
    if (ok) *ok = true;
    int last = -1;
    for (int n = 15; n >= 0; --n)
        if (coeff[n] != 0) { last = n; break; }
    std::vector<Bin> o;
    if (last < 0) return o;  // cbf=0，调用方不进入本函数

    const auto& st = scan4x4_tables(scan_idx);
    const int cx = st.x[last], cy = st.y[last];
    // last_significant_coeff_x/y_prefix：4x4 时 ctx_shift=0、ctx_offset=0、max=3，
    // prefix<=3 无 suffix，unary 编码。SCAN_VERT 时解码端会 FFSWAP(x,y)
    // （FFmpeg cabac.c:1222-1223），故编码端必须预先交换写入顺序。
    const int lx = st.swap_last_xy ? cy : cx;
    const int ly = st.swap_last_xy ? cx : cy;
    for (int i = 0; i < lx; ++i) o.push_back({kCtxLastSigCoeffX + i, 1});
    if (lx < 3) o.push_back({kCtxLastSigCoeffX + lx, 0});
    for (int i = 0; i < ly; ++i) o.push_back({kCtxLastSigCoeffY + i, 1});
    if (ly < 3) o.push_back({kCtxLastSigCoeffY + ly, 0});

    // significant_coeff_flag：单 CG，last 位置隐含为 1 不写。
    int idx[16] = {};
    int nb = 1;
    idx[0] = last;
    // FFmpeg: n_end = last_scan_pos - 1；当 last==0 时 n_end<0，
    // significant_coeff_flag 整块不执行，位置 0 就是隐含的 last 系数。
    if (last >= 1) {
        for (int n = last - 1; n > 0; --n) {
            const int sig = coeff[n] != 0 ? 1 : 0;
            o.push_back({kCtxSigCoeffFlag + st.ctx_map[n], sig});
            if (sig) idx[nb++] = n;
        }
        const int sig0 = coeff[0] != 0 ? 1 : 0;
        o.push_back({kCtxSigCoeffFlag, sig0});  // decode_0 用 93 + scf_offset(0)
        if (sig0) idx[nb++] = 0;
    }

    // coeff_abs_level_greater1_flag（ctx_set=0，最多前 8 个）。
    int gt1[16] = {};
    int gctx = 1, mask = 0;
    const int m8 = nb < 8 ? nb : 8;
    for (int m = 0; m < m8; ++m) {
        const int f = abs_level(coeff[idx[m]]) > 1 ? 1 : 0;
        o.push_back({kCtxCoeffAbsGreater1 + gctx, f});
        gt1[m] = f;
        if (f) mask |= 1 << m;
        gctx = next_greater1_ctx(gctx, f);
    }
    int first_gt1 = -1;
    for (int m = 0; m < m8; ++m)
        if (mask & (1 << m)) { first_gt1 = m; break; }
    int gt2 = 0;
    if (first_gt1 >= 0) {
        gt2 = abs_level(coeff[idx[first_gt1]]) > 2 ? 1 : 0;
        o.push_back({kCtxCoeffAbsGreater2, gt2});
    }

    // sign_data_hiding_flag=0：每个 significant 系数一个 bypass 符号位。
    for (int m = 0; m < nb; ++m)
        o.push_back({kBypassCtx, coeff[idx[m]] < 0 ? 1 : 0});

    // coeff_abs_level_remaining：Rice，k 按 FFmpeg 规则逐步升级。
    int k = 0;
    for (int m = 0; m < nb; ++m) {
        int tcl;
        bool write_rem;
        level_base(m, first_gt1, gt1, gt2, m8, tcl, write_rem);
        if (!write_rem) continue;
        const int a = abs_level(coeff[idx[m]]);
        const int rem = a - tcl;
        int prefix = rem >> k;
        int suffix = rem & ((1 << k) - 1);
        int suffix_bits = k;
        if (prefix >= 3) {
            // FFmpeg coeff_abs_level_remaining_decode 的 escape 分支：
            // remaining = (((1<<pm3)+2)<<k) + suffix，suffix 位宽 pm3+k。
            int pm3 = 0;
            for (;;) {
                const int base = ((1 << pm3) + 2) << k;
                const int span = 1 << (pm3 + k);
                if (rem >= base && rem < base + span) {
                    suffix = rem - base;
                    suffix_bits = pm3 + k;
                    prefix = pm3 + 3;
                    break;
                }
                if (++pm3 > 20) { if (ok) *ok = false; return {}; }
            }
            if (prefix >= 31) { if (ok) *ok = false; return {}; }  // CABAC_MAX_BIN
        }
        for (int i = 0; i < prefix; ++i) o.push_back({kBypassCtx, 1});
        o.push_back({kBypassCtx, 0});
        for (int i = suffix_bits - 1; i >= 0; --i)
            o.push_back({kBypassCtx, (suffix >> i) & 1});
        if (a > (3 << k) && k < 4) ++k;
    }
    return o;
}

bool decode_residual_4x4(const Bin* bins, std::size_t count, std::int16_t* coeff,
                         std::size_t* consumed, ScanIdx scan_idx) {
    for (int i = 0; i < 16; ++i) coeff[i] = 0;
    const auto& st = scan4x4_tables(scan_idx);
    Reader r{bins, count};
    int lx = 0, ly = 0, v = 0;
    while (lx < 3) {
        if (!r.decision(kCtxLastSigCoeffX + lx, v)) return false;
        if (!v) break;
        ++lx;
    }
    while (ly < 3) {
        if (!r.decision(kCtxLastSigCoeffY + ly, v)) return false;
        if (!v) break;
        ++ly;
    }
    if (st.swap_last_xy) std::swap(lx, ly);  // 与编码端预交换对偶
    const int last = st.inv[ly][lx];

    int idx[16] = {};
    int nb = 1;
    idx[0] = last;
    if (last >= 1) {
        for (int n = last - 1; n > 0; --n) {
            if (!r.decision(kCtxSigCoeffFlag + st.ctx_map[n], v)) return false;
            if (v) idx[nb++] = n;
        }
        if (!r.decision(kCtxSigCoeffFlag, v)) return false;
        if (v) idx[nb++] = 0;
    }

    int gt1[16] = {};
    int gctx = 1, mask = 0;
    const int m8 = nb < 8 ? nb : 8;
    for (int m = 0; m < m8; ++m) {
        if (!r.decision(kCtxCoeffAbsGreater1 + gctx, v)) return false;
        gt1[m] = v;
        if (v) mask |= 1 << m;
        gctx = next_greater1_ctx(gctx, v);
    }
    int first_gt1 = -1;
    for (int m = 0; m < m8; ++m)
        if (mask & (1 << m)) { first_gt1 = m; break; }
    int gt2 = 0;
    if (first_gt1 >= 0 && !r.decision(kCtxCoeffAbsGreater2, gt2)) return false;

    int sign[16] = {};
    for (int m = 0; m < nb; ++m)
        if (!r.bypass(sign[m])) return false;

    int k = 0;
    for (int m = 0; m < nb; ++m) {
        int tcl;
        bool write_rem;
        level_base(m, first_gt1, gt1, gt2, m8, tcl, write_rem);
        int a = tcl;
        if (write_rem) {
            int prefix = 0;
            for (;;) {
                if (!r.bypass(v)) return false;
                if (!v) break;
                ++prefix;
            }
            const int bits = (prefix < 3) ? k : (prefix - 3 + k);
            int suffix = 0;
            for (int i = 0; i < bits; ++i) {
                if (!r.bypass(v)) return false;
                suffix = (suffix << 1) | v;
            }
            const int rem = (prefix < 3)
                                ? (prefix << k) + suffix
                                : ((((1 << (prefix - 3)) + 2) << k) + suffix);
            a = tcl + rem;
            if (a > (3 << k) && k < 4) ++k;
        }
        coeff[idx[m]] = static_cast<std::int16_t>(sign[m] ? -a : a);
    }
    if (consumed) *consumed = r.p;
    return true;
}

}  // namespace termchart::image::codec::hevc
