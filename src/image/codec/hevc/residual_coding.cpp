// residual_coding.cpp —— 4x4 TU 残差语法（按 FFmpeg ff_hevc_hls_residual_coding 翻译）
#include "residual_coding.hpp"

#include <cstdlib>

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

// FFmpeg decode_residual 的 greater1_ctx 演化。
inline int next_greater1_ctx(int g, int flag) {
    if (flag) return 0;
    return g < 3 ? g + 1 : 3;
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

std::vector<Bin> encode_residual_4x4(const std::int16_t* coeff, bool* ok) {
    if (ok) *ok = true;
    int last = -1;
    for (int n = 15; n >= 0; --n)
        if (coeff[n] != 0) { last = n; break; }
    std::vector<Bin> o;
    if (last < 0) return o;  // cbf=0，调用方不进入本函数

    const int lx = kDiagScan4x4X[last], ly = kDiagScan4x4Y[last];
    // last_significant_coeff_x/y_prefix：4x4 时 ctx_shift=0、ctx_offset=0、max=3，
    // prefix<=3 无 suffix，unary 编码。
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
            o.push_back({kCtxSigCoeffFlag + kSigCoeffCtxMap4x4[n], sig});
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
                         std::size_t* consumed) {
    for (int i = 0; i < 16; ++i) coeff[i] = 0;
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
    const int last = kDiagScan4x4Inv[ly][lx];

    int idx[16] = {};
    int nb = 1;
    idx[0] = last;
    if (last >= 1) {
        for (int n = last - 1; n > 0; --n) {
            if (!r.decision(kCtxSigCoeffFlag + kSigCoeffCtxMap4x4[n], v)) return false;
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
