// residual_coding.cpp —— 4x4 TU 残差语法（按 FFmpeg ff_hevc_hls_residual_coding 翻译）
#include "residual_coding.hpp"

#include <array>
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

// FFmpeg cabac.c:1226-1232 的 ctx_idx_map：5 个 16 项块，索引 (y_c<<2)+x_c。
// 块 0 专供 log2_trafo_size==2（三套扫描共用它：坐标本身随 scan_idx 互换，
// 等价于规格 9-44/9-45/9-46）；块 1..3 按 prev_sig+1 选，块 4 是 default（全 2）。
const std::uint8_t kCtxIdxMap[5][16] = {
    {0, 1, 4, 5, 2, 3, 4, 5, 6, 6, 8, 8, 7, 7, 8, 8},
    {1, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0},
    {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
};

// 对角线扫描：d=x+y 递增，同一条对角线内 x 递增（y 递减）。4x4 时逐项等于
// kDiagScan4x4，2x2/8x8 时等于 FFmpeg diag_scan2x2_* / ff_hevc_diag_scan8x8_*。
void fill_diag_scan(int n, std::uint8_t* x, std::uint8_t* y) {
    int p = 0;
    for (int d = 0; d < 2 * n - 1; ++d) {
        const int x0 = d - (n - 1) > 0 ? d - (n - 1) : 0;
        const int x1 = d < n - 1 ? d : n - 1;
        for (int cx = x0; cx <= x1; ++cx) {
            x[p] = static_cast<std::uint8_t>(cx);
            y[p] = static_cast<std::uint8_t>(d - cx);
            ++p;
        }
    }
}

// 行优先 / 列优先扫描（大 TU 只 SCAN_HORIZ/VERT 的 8x8 用它）。
void fill_row_scan(int n, std::uint8_t* x, std::uint8_t* y) {
    for (int p = 0; p < n * n; ++p) {
        x[p] = static_cast<std::uint8_t>(p % n);
        y[p] = static_cast<std::uint8_t>(p / n);
    }
}

void fill_col_scan(int n, std::uint8_t* x, std::uint8_t* y) {
    for (int p = 0; p < n * n; ++p) {
        x[p] = static_cast<std::uint8_t>(p / n);
        y[p] = static_cast<std::uint8_t>(p % n);
    }
}
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

// 大 TU（log2_size 3..5）的扫描表：CG 网格 ncg×ncg（ncg = size/4 ∈ {2,4,8}），
// 每个 CG 内仍是 4x4 子扫描。数组按 32x32 的最大尺寸铺满（CG 最多 64 个）。
struct TuScan {
    std::uint8_t ncg = 1;
    std::uint8_t cg_x[64] = {};   // CG 扫描位置 -> CG 的 xCG
    std::uint8_t cg_y[64] = {};   // CG 扫描位置 -> CG 的 yCG
    std::uint8_t inv_cg[64] = {};  // [yCG*8 + xCG] -> CG 扫描位置
    std::uint8_t off_x[16] = {};   // CG 内扫描位置 -> 4x4 的 x_c
    std::uint8_t off_y[16] = {};   // CG 内扫描位置 -> 4x4 的 y_c
    std::uint8_t inv_off[16] = {}; // [y_c*4 + x_c] -> CG 内扫描位置
};

// 表只构造一次（magic static），按 [log2-2][scan_idx] 索引。
const TuScan& tu_scan(int log2_size, ScanIdx idx) {
    static const std::array<std::array<TuScan, 3>, 4> kAll = [] {
        std::array<std::array<TuScan, 3>, 4> a{};
        for (int l = 2; l <= 5; ++l) {
            const int n = (1 << l) >> 2;  // CG 网格边长
            for (int s = 0; s < 3; ++s) {
                TuScan& t = a[static_cast<std::size_t>(l - 2)][static_cast<std::size_t>(s)];
                t.ncg = static_cast<std::uint8_t>(n);
                if (s == 0) fill_diag_scan(n, t.cg_x, t.cg_y);
                else if (s == 1) fill_row_scan(n, t.cg_x, t.cg_y);
                else fill_col_scan(n, t.cg_x, t.cg_y);
                for (int p = 0; p < n * n; ++p)
                    t.inv_cg[(t.cg_y[p] << 3) + t.cg_x[p]] = static_cast<std::uint8_t>(p);
                if (s == 0) fill_diag_scan(4, t.off_x, t.off_y);
                else if (s == 1) fill_row_scan(4, t.off_x, t.off_y);
                else fill_col_scan(4, t.off_x, t.off_y);
                for (int p = 0; p < 16; ++p)
                    t.inv_off[(t.off_y[p] << 2) + t.off_x[p]] = static_cast<std::uint8_t>(p);
            }
        }
        return a;
    }();
    const int l = log2_size < 2 ? 0 : (log2_size > 5 ? 3 : log2_size - 2);
    const auto si = static_cast<std::size_t>(idx == kScanHoriz ? 1 : (idx == kScanVert ? 2 : 0));
    return kAll[static_cast<std::size_t>(l)][si];
}


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

std::vector<Bin> encode_residual_4x4(const std::int16_t* coeff, bool* ok,
                                     ScanIdx scan_idx, int c_idx) {
    if (ok) *ok = true;
    // FFmpeg cabac.c / log2_trafo_size==2 的色度上下文偏移（c_idx>0）：
    //   last_significant_coeff_x/y_prefix ctx_offset 15（亮度 0）
    //   significant_coeff_flag scf_offset 27、greater1 +16、greater2 +4
    const int d_last = c_idx ? 15 : 0;
    const int d_sig  = c_idx ? 27 : 0;
    const int d_gt1  = c_idx ? 16 : 0;
    const int d_gt2  = c_idx ? 4 : 0;

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
    for (int i = 0; i < lx; ++i) o.push_back({kCtxLastSigCoeffX + d_last + i, 1});
    if (lx < 3) o.push_back({kCtxLastSigCoeffX + d_last + lx, 0});
    for (int i = 0; i < ly; ++i) o.push_back({kCtxLastSigCoeffY + d_last + i, 1});
    if (ly < 3) o.push_back({kCtxLastSigCoeffY + d_last + ly, 0});

    // significant_coeff_flag：单 CG，last 位置隐含为 1 不写。
    int idx[16] = {};
    int nb = 1;
    idx[0] = last;
    // FFmpeg: n_end = last_scan_pos - 1；当 last==0 时 n_end<0，
    // significant_coeff_flag 整块不执行，位置 0 就是隐含的 last 系数。
    if (last >= 1) {
        for (int n = last - 1; n > 0; --n) {
            const int sig = coeff[n] != 0 ? 1 : 0;
            o.push_back({kCtxSigCoeffFlag + d_sig + st.ctx_map[n], sig});
            if (sig) idx[nb++] = n;
        }
        const int sig0 = coeff[0] != 0 ? 1 : 0;
        o.push_back({kCtxSigCoeffFlag + d_sig, sig0});  // 色度 +27
        if (sig0) idx[nb++] = 0;
    }

    // coeff_abs_level_greater1_flag（ctx_set=0，最多前 8 个）。
    int gt1[16] = {};
    int gctx = 1, mask = 0;
    const int m8 = nb < 8 ? nb : 8;
    for (int m = 0; m < m8; ++m) {
        const int f = abs_level(coeff[idx[m]]) > 1 ? 1 : 0;
        o.push_back({kCtxCoeffAbsGreater1 + d_gt1 + gctx, f});
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
        o.push_back({kCtxCoeffAbsGreater2 + d_gt2, gt2});
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
                         std::size_t* consumed, ScanIdx scan_idx, int c_idx) {
    for (int i = 0; i < 16; ++i) coeff[i] = 0;
    // FFmpeg cabac.c / log2_trafo_size==2 的色度上下文偏移（c_idx>0）：
    //   last_significant_coeff_x/y_prefix ctx_offset 15（亮度 0）
    //   significant_coeff_flag scf_offset 27、greater1 +16、greater2 +4
    const int d_last = c_idx ? 15 : 0;
    const int d_sig  = c_idx ? 27 : 0;
    const int d_gt1  = c_idx ? 16 : 0;
    const int d_gt2  = c_idx ? 4 : 0;

    const auto& st = scan4x4_tables(scan_idx);
    Reader r{bins, count};
    int lx = 0, ly = 0, v = 0;
    while (lx < 3) {
        if (!r.decision(kCtxLastSigCoeffX + d_last + lx, v)) return false;
        if (!v) break;
        ++lx;
    }
    while (ly < 3) {
        if (!r.decision(kCtxLastSigCoeffY + d_last + ly, v)) return false;
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
            if (!r.decision(kCtxSigCoeffFlag + d_sig + st.ctx_map[n], v)) return false;
            if (v) idx[nb++] = n;
        }
        if (!r.decision(kCtxSigCoeffFlag + d_sig, v)) return false;
        if (v) idx[nb++] = 0;
    }

    int gt1[16] = {};
    int gctx = 1, mask = 0;
    const int m8 = nb < 8 ? nb : 8;
    for (int m = 0; m < m8; ++m) {
        if (!r.decision(kCtxCoeffAbsGreater1 + d_gt1 + gctx, v)) return false;
        gt1[m] = v;
        if (v) mask |= 1 << m;
        gctx = next_greater1_ctx(gctx, v);
    }
    int first_gt1 = -1;
    for (int m = 0; m < m8; ++m)
        if (mask & (1 << m)) { first_gt1 = m; break; }
    int gt2 = 0;
    if (first_gt1 >= 0 && !r.decision(kCtxCoeffAbsGreater2 + d_gt2, gt2)) return false;

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

// ═══ 大 TU（log2_size 3..5）残差语法 ═══
namespace {

// 扫描位置 (p,q) → 块内坐标。表本身已承载 scan_idx 的方向：SCAN_VERT 用转置
// 表，配合 last_sig 的 FFSWAP 后，表给出的 (x,y) 就是 FFmpeg GET_COORD 的
// (x_c,y_c)，**不再额外交换**（多交换一次会把系数整体转置）。
inline void scan_coord(const TuScan& t, bool vert, int p, int q, int& x, int& y) {
    (void)vert;
    x = 4 * t.cg_x[p] + t.off_x[q];
    y = 4 * t.cg_y[p] + t.off_y[q];
}

// last_significant_coeff_x/y_prefix 的 ctx_offset / ctx_shift / 上界。
inline void last_sig_ctx(int log2_size, int c_idx, int& off, int& shift, int& maxv) {
    maxv = 2 * log2_size - 1;
    if (c_idx) {
        off = 15;
        shift = log2_size - 2;
    } else {
        off = 3 * (log2_size - 2) + ((log2_size - 1) >> 2);
        shift = (log2_size + 1) >> 2;
    }
}

// Rice 主体：把 rem 的 unary 前缀 + 后缀位写进 o（对齐 coeff_abs_level_remaining_decode）。
inline int emit_remaining(int rem, int k, std::vector<Bin>& o) {
    int prefix = rem >> k;
    int suffix = rem & ((1 << k) - 1);
    int suffix_bits = k;
    if (prefix >= 3) {
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
            if (++pm3 > 20) return -1;
        }
    }
    if (prefix >= 31) return -1;  // CABAC_MAX_BIN
    for (int i = 0; i < prefix; ++i) o.push_back({kBypassCtx, 1});
    o.push_back({kBypassCtx, 0});
    for (int i = suffix_bits - 1; i >= 0; --i) o.push_back({kBypassCtx, (suffix >> i) & 1});
    return 0;
}

}  // namespace

std::vector<Bin> encode_residual(const std::int16_t* coeff, int log2_size, bool* ok,
                                 ScanIdx scan_idx, int c_idx) {
    if (ok) *ok = true;
    if (!coeff || log2_size < 2 || log2_size > 5) {
        if (ok) *ok = false;
        return {};
    }
    const int size = 1 << log2_size;
    if (log2_size == 2) {  // 交给已验证的单 CG 实现，保证逐 bin 一致
        const auto& st = scan4x4_tables(scan_idx);
        std::int16_t sc16[16];
        for (int n = 0; n < 16; ++n) sc16[n] = coeff[st.y[n] * 4 + st.x[n]];
        return encode_residual_4x4(sc16, ok, scan_idx, c_idx);
    }
    if (log2_size > 3) scan_idx = kScanDiag;  // FFmpeg 只对 log2<4 按帧内模式改 scan
    const bool vert = scan_idx == kScanVert;
    const TuScan& t = tu_scan(log2_size, scan_idx);
    const int ncg = t.ncg, nsc = ncg * ncg;

    std::vector<std::int16_t> sc(static_cast<std::size_t>(nsc) * 16, 0);
    auto at = [&](int p, int q) -> std::int16_t& {
        return sc[static_cast<std::size_t>(p) * 16 + static_cast<std::size_t>(q)];
    };
    for (int p = 0; p < nsc; ++p)
        for (int q = 0; q < 16; ++q) {
            int x, y;
            scan_coord(t, vert, p, q, x, y);
            at(p, q) = coeff[y * size + x];
        }
    int last = -1;
    for (int i = nsc * 16 - 1; i >= 0; --i)
        if (sc[static_cast<std::size_t>(i)] != 0) { last = i; break; }
    std::vector<Bin> o;
    if (last < 0) return o;

    const int p_last = last >> 4, q_last = last & 15;
    const int last_sx = 4 * t.cg_x[p_last] + t.off_x[q_last];
    const int last_sy = 4 * t.cg_y[p_last] + t.off_y[q_last];
    int last_off, last_shift, last_max;
    last_sig_ctx(log2_size, c_idx, last_off, last_shift, last_max);
    // FFmpeg cabac.c:1112-1127：先把 x/y 两个 prefix 都解完，再依次解
    // x_suffix、y_suffix。顺序写反会让解码端把 bypass 位当 ctx bin 读，
    // 算术状态立刻跑偏（4x4 无 suffix 才一直没暴露）。
    struct LastPlan { int prefix = 0, suffix = 0, len = 0; };
    auto plan_last = [&](int coord) {
        LastPlan r;
        r.prefix = coord;
        if (coord > 3) {
            for (int p = 4; p <= last_max; ++p) {
                const int l = (p >> 1) - 1;
                const int base = (2 + (p & 1)) << l;
                const int span = 1 << l;
                if (coord >= base && coord < base + span) {
                    r.prefix = p;
                    r.len = l;
                    r.suffix = coord - base;
                    break;
                }
            }
        }
        return r;
    };
    // SCAN_VERT 时解码端在解出两个 prefix 后 FFSWAP(x,y)，故写入顺序先交换。
    const LastPlan px = plan_last(vert ? last_sy : last_sx);
    const LastPlan py = plan_last(vert ? last_sx : last_sy);
    auto emit_prefix = [&](int ctx_base, const LastPlan& r) {
        for (int i = 0; i < r.prefix; ++i)
            o.push_back({ctx_base + last_off + (i >> last_shift), 1});
        if (r.prefix < last_max)
            o.push_back({ctx_base + last_off + (r.prefix >> last_shift), 0});
    };
    auto emit_suffix = [&](const LastPlan& r) {
        for (int b = r.len - 1; b >= 0; --b)
            o.push_back({kBypassCtx, (r.suffix >> b) & 1});
    };
    emit_prefix(kCtxLastSigCoeffX, px);
    emit_prefix(kCtxLastSigCoeffY, py);
    emit_suffix(px);
    emit_suffix(py);

    std::uint8_t cflag[64] = {};
    int greater1_ctx = 1;  // 跨 CG 保持
    // FFmpeg 的循环是 for (i = num_last_subset; i >= 0; i--)：扫描序在 last CG
    // 之后的 CG 根本不进语法（系数隐含全零），从 nsc-1 起写会整体错位。
    for (int p = p_last; p >= 0; --p) {
        const int ax = t.cg_x[p], ay = t.cg_y[p];
        const bool inferred = (p == p_last || p == 0);  // 这两处组标志推断为 1
        int flag = 1;
        if (!inferred) {
            flag = 0;
            for (int q = 0; q < 16; ++q)
                if (at(p, q) != 0) { flag = 1; break; }
        }
        cflag[(ay << 3) + ax] = static_cast<std::uint8_t>(flag);
        if (!inferred) {
            int ctx_cg = 0;
            if (ax + 1 < ncg) ctx_cg += cflag[(ay << 3) + ax + 1];
            if (ay + 1 < ncg) ctx_cg += cflag[((ay + 1) << 3) + ax];
            o.push_back({kCtxSigCoeffGroup + (ctx_cg > 1 ? 1 : ctx_cg) + (c_idx ? 2 : 0), flag});
        }
        int prev_sig = 0;
        if (ax + 1 < ncg && cflag[(ay << 3) + ax + 1]) prev_sig = 1;
        if (ay + 1 < ncg && cflag[((ay + 1) << 3) + ax]) prev_sig += 2;

        const int n_end = (p == p_last) ? q_last - 1 : 15;
        int idx[16] = {};
        int nb = 0;
        if (p == p_last) idx[nb++] = q_last;
        if (flag && n_end >= 0) {
            int scf = (c_idx ? 27 : 0);
            const std::uint8_t* map = &kCtxIdxMap[prev_sig + 1][0];
            if (c_idx == 0) {
                if (ax > 0 || ay > 0) scf += 3;
                if (log2_size == 3) scf += (scan_idx == kScanDiag) ? 9 : 15;
                else scf += 21;
            } else {
                scf += (log2_size == 3) ? 9 : 12;
            }
            bool implicit0 = (p > 0 && p < p_last);
            for (int q = n_end; q > 0; --q) {
                const int sig = at(p, q) != 0 ? 1 : 0;
                o.push_back({kCtxSigCoeffFlag + scf + map[(t.off_y[q] << 2) + t.off_x[q]], sig});
                if (sig) { idx[nb++] = q; implicit0 = false; }
            }
            if (implicit0) {
                idx[nb++] = 0;
            } else {
                const int scf0 = (p == 0) ? (c_idx ? 27 : 0) : scf + 2;
                const int sig0 = at(p, 0) != 0 ? 1 : 0;
                o.push_back({kCtxSigCoeffFlag + scf0, sig0});
                if (sig0) idx[nb++] = 0;
            }
        }
        if (nb <= 0) continue;

        const int m8 = nb < 8 ? nb : 8;
        int ctx_set = (p > 0 && c_idx == 0) ? 2 : 0;
        if (!(p == p_last) && greater1_ctx == 0) ++ctx_set;
        greater1_ctx = 1;
        const int g1_base = kCtxCoeffAbsGreater1 + (c_idx ? 16 : 0) + (ctx_set << 2);
        int gt1[16] = {}, mask = 0;
        for (int m = 0; m < m8; ++m) {
            const int f = abs_level(at(p, idx[m])) > 1 ? 1 : 0;
            o.push_back({g1_base + greater1_ctx, f});
            gt1[m] = f;
            if (f) mask |= 1 << m;
            greater1_ctx = next_greater1_ctx(greater1_ctx, f);
        }
        int first_gt1 = -1;
        for (int m = 0; m < m8; ++m)
            if (mask & (1 << m)) { first_gt1 = m; break; }
        int gt2 = 0;
        if (first_gt1 >= 0) {
            gt2 = abs_level(at(p, idx[first_gt1])) > 2 ? 1 : 0;
            o.push_back({kCtxCoeffAbsGreater2 + (c_idx ? 4 : 0) + ctx_set, gt2});
        }
        for (int m = 0; m < nb; ++m)  // sign_data_hiding=0
            o.push_back({kBypassCtx, at(p, idx[m]) < 0 ? 1 : 0});
        int k = 0;
        for (int m = 0; m < nb; ++m) {
            int tcl;
            bool write_rem;
            level_base(m, first_gt1, gt1, gt2, m8, tcl, write_rem);
            if (!write_rem) continue;
            const int a = abs_level(at(p, idx[m]));
            if (emit_remaining(a - tcl, k, o) != 0) {
                if (ok) *ok = false;
                return {};
            }
            if (a > (3 << k) && k < 4) ++k;
        }
    }
    return o;
}

bool decode_residual(const Bin* bins, std::size_t count, std::int16_t* coeff, int log2_size,
                     std::size_t* consumed, ScanIdx scan_idx, int c_idx) {
    if (!coeff || log2_size < 2 || log2_size > 5) return false;
    const int size = 1 << log2_size;
    for (int i = 0; i < size * size; ++i) coeff[i] = 0;
    if (log2_size == 2) {
        const auto& st = scan4x4_tables(scan_idx);
        std::int16_t sc16[16];
        if (!decode_residual_4x4(bins, count, sc16, consumed, scan_idx, c_idx)) return false;
        for (int n = 0; n < 16; ++n) coeff[st.y[n] * 4 + st.x[n]] = sc16[n];
        return true;
    }
    if (log2_size > 3) scan_idx = kScanDiag;
    const bool vert = scan_idx == kScanVert;
    const TuScan& t = tu_scan(log2_size, scan_idx);
    const int ncg = t.ncg;

    int off, shift, maxv;
    last_sig_ctx(log2_size, c_idx, off, shift, maxv);
    Reader r{bins, count};
    // 与编码端对偶：先读两个 prefix，再依次读 x/y 的 suffix。
    auto read_prefix = [&](int ctx_base) {
        int v = 0;
        while (v < maxv) {
            int b;
            if (!r.decision(ctx_base + off + (v >> shift), b)) return -1;
            if (!b) break;
            ++v;
        }
        return v;
    };
    auto read_suffix = [&](int prefix) {
        if (prefix <= 3) return 0;
        const int len = (prefix >> 1) - 1;
        int val = 0;
        for (int i = 0; i < len; ++i) {
            int b;
            if (!r.bypass(b)) return -1;
            val = (val << 1) | b;
        }
        return val;
    };
    const int px = read_prefix(kCtxLastSigCoeffX);
    const int py = read_prefix(kCtxLastSigCoeffY);
    if (px < 0 || py < 0) return false;
    int lx = px, ly = py;
    if (px > 3) {
        const int sx = read_suffix(px);
        if (sx < 0) return false;
        lx = ((2 + (px & 1)) << ((px >> 1) - 1)) + sx;
    }
    if (py > 3) {
        const int sy = read_suffix(py);
        if (sy < 0) return false;
        ly = ((2 + (py & 1)) << ((py >> 1) - 1)) + sy;
    }
    if (vert) std::swap(lx, ly);  // FFmpeg cabac.c:1129-1130 的 FFSWAP
    const int p_last = t.inv_cg[((ly >> 2) << 3) + (lx >> 2)];
    const int q_last = t.inv_off[((ly & 3) << 2) + (lx & 3)];

    std::uint8_t cflag[64] = {};
    int greater1_ctx = 1;
    for (int p = p_last; p >= 0; --p) {
        const int ax = t.cg_x[p], ay = t.cg_y[p];
        const bool inferred = (p == p_last || p == 0);
        int flag = 1;
        if (!inferred) {
            int ctx_cg = 0;
            if (ax + 1 < ncg) ctx_cg += cflag[(ay << 3) + ax + 1];
            if (ay + 1 < ncg) ctx_cg += cflag[((ay + 1) << 3) + ax];
            int v;
            if (!r.decision(kCtxSigCoeffGroup + (ctx_cg > 1 ? 1 : ctx_cg) + (c_idx ? 2 : 0), v))
                return false;
            flag = v;
        }
        cflag[(ay << 3) + ax] = static_cast<std::uint8_t>(flag);
        int prev_sig = 0;
        if (ax + 1 < ncg && cflag[(ay << 3) + ax + 1]) prev_sig = 1;
        if (ay + 1 < ncg && cflag[((ay + 1) << 3) + ax]) prev_sig += 2;

        const int n_end = (p == p_last) ? q_last - 1 : 15;
        int idx[16] = {};
        int nb = 0;
        if (p == p_last) idx[nb++] = q_last;
        if (flag && n_end >= 0) {
            int scf = (c_idx ? 27 : 0);
            const std::uint8_t* map = &kCtxIdxMap[prev_sig + 1][0];
            if (c_idx == 0) {
                if (ax > 0 || ay > 0) scf += 3;
                if (log2_size == 3) scf += (scan_idx == kScanDiag) ? 9 : 15;
                else scf += 21;
            } else {
                scf += (log2_size == 3) ? 9 : 12;
            }
            bool implicit0 = (p > 0 && p < p_last);
            for (int q = n_end; q > 0; --q) {
                int v;
                if (!r.decision(kCtxSigCoeffFlag + scf + map[(t.off_y[q] << 2) + t.off_x[q]], v))
                    return false;
                if (v) { idx[nb++] = q; implicit0 = false; }
            }
            if (implicit0) {
                idx[nb++] = 0;
            } else {
                const int scf0 = (p == 0) ? (c_idx ? 27 : 0) : scf + 2;
                int v;
                if (!r.decision(kCtxSigCoeffFlag + scf0, v)) return false;
                if (v) idx[nb++] = 0;
            }
        }
        if (nb <= 0) continue;

        const int m8 = nb < 8 ? nb : 8;
        int ctx_set = (p > 0 && c_idx == 0) ? 2 : 0;
        if (!(p == p_last) && greater1_ctx == 0) ++ctx_set;
        greater1_ctx = 1;
        const int g1_base = kCtxCoeffAbsGreater1 + (c_idx ? 16 : 0) + (ctx_set << 2);
        int gt1[16] = {}, mask = 0;
        for (int m = 0; m < m8; ++m) {
            int v;
            if (!r.decision(g1_base + greater1_ctx, v)) return false;
            gt1[m] = v;
            if (v) mask |= 1 << m;
            greater1_ctx = next_greater1_ctx(greater1_ctx, v);
        }
        int first_gt1 = -1;
        for (int m = 0; m < m8; ++m)
            if (mask & (1 << m)) { first_gt1 = m; break; }
        int gt2 = 0;
        if (first_gt1 >= 0 &&
            !r.decision(kCtxCoeffAbsGreater2 + (c_idx ? 4 : 0) + ctx_set, gt2))
            return false;

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
                int prefix = 0, v;
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
                a = tcl + ((prefix < 3) ? ((prefix << k) + suffix)
                                        : ((((1 << (prefix - 3)) + 2) << k) + suffix));
                if (a > (3 << k) && k < 4) ++k;
            }
            int x, y;
            scan_coord(t, vert, p, idx[m], x, y);
            coeff[y * size + x] = static_cast<std::int16_t>(sign[m] ? -a : a);
        }
    }
    if (consumed) *consumed = r.p;
    return true;
}

}  // namespace termchart::image::codec::hevc

