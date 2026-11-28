// sao.cpp —— HEVC SAO：语法 bin 化 / 回读 / 像素滤波 / 编码端 RD 决策
#include "sao.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace termchart::image::codec::hevc {
namespace {

// 8bit 时 offset_abs 的截断一元上界：length = (1 << (min(bit_depth,10)-5)) - 1 = 7。
constexpr int kOffsetAbsMax = 7;

void put_offset_abs(std::vector<Bin>& out, int abs_val) {
    for (int i = 0; i < kOffsetAbsMax; ++i) {
        if (i < abs_val) {
            out.push_back({kBypassCtx, 1});
        } else {
            out.push_back({kBypassCtx, 0});
            return;
        }
    }
}

bool get_offset_abs(BinCursor& c, int& abs_val) {
    abs_val = 0;
    while (abs_val < kOffsetAbsMax) {
        int b = 0;
        if (!c.bypass(b)) return false;
        if (!b) return true;
        ++abs_val;
    }
    return true;
}

void put_type_idx(std::vector<Bin>& out, int type) {
    out.push_back({kCtxSaoTypeIdx, type == kSaoNotApplied ? 0 : 1});
    if (type == kSaoNotApplied) return;
    out.push_back({kBypassCtx, type == kSaoEdge ? 1 : 0});
}

bool get_type_idx(BinCursor& c, int& type) {
    int b = 0;
    if (!c.decision(kCtxSaoTypeIdx, b)) return false;
    if (!b) {
        type = kSaoNotApplied;
        return true;
    }
    if (!c.bypass(b)) return false;
    type = b ? kSaoEdge : kSaoBand;
    return true;
}

void put_band_position(std::vector<Bin>& out, int pos) {
    for (int i = 4; i >= 0; --i) out.push_back({kBypassCtx, (pos >> i) & 1});
}

bool get_band_position(BinCursor& c, int& pos) {
    pos = 0;
    for (int i = 0; i < 5; ++i) {
        int b = 0;
        if (!c.bypass(b)) return false;
        pos = (pos << 1) | b;
    }
    return true;
}

void put_eo_class(std::vector<Bin>& out, int cls) {
    out.push_back({kBypassCtx, (cls >> 1) & 1});
    out.push_back({kBypassCtx, cls & 1});
}

bool get_eo_class(BinCursor& c, int& cls) {
    int hi = 0, lo = 0;
    if (!c.bypass(hi) || !c.bypass(lo)) return false;
    cls = (hi << 1) | lo;
    return true;
}

// BAND 的 offset 全是带符号值，EDGE 的符号由类别固定（[+, +, 0, -, -]）。
void derive_offsets_impl(SaoCtbParams& p) {
    for (int c = 0; c < 3; ++c) {
        p.offset_val[c][0] = 0;
        for (int i = 0; i < 4; ++i) {
            int v = p.offset_abs[c][i];
            // EDGE 的符号由 edgeIdx 固定：abs[0]/abs[1]（edgeIdx 0/1）取正，
            // abs[2]/abs[3]（edgeIdx 3/4）取负；edgeIdx 2 落在未使用的
            // offset_val[0] 上（恒 0，见 kEdgeIdx）。与 FFmpeg 推导逐条一致
            // （hevc/hevcdec.c: `if (i > 1) negate`）。
            if (p.type_idx[c] == kSaoEdge) {
                if (i > 1) v = -v;
            } else if (p.offset_sign[c][i]) {
                v = -v;
            }
            p.offset_val[c][i + 1] = v;
        }
    }
}

// 8.7.3.4：edgeIdx 类别（2+sign+sign，0..4）→ offset_val 下标。
constexpr int kEdgeIdx[5] = {1, 2, 0, 3, 4};

// 与 FFmpeg h2656_sao_template.c 的 eo_class 邻居方向逐条一致。
void eo_neighbours(int cls, int& dx1, int& dy1, int& dx2, int& dy2) {
    switch (cls) {
        case 0:  // 水平
            dx1 = -1; dy1 = 0; dx2 = 1; dy2 = 0; break;
        case 1:  // 垂直
            dx1 = 0; dy1 = -1; dx2 = 0; dy2 = 1; break;
        case 2:  // 135°（左上 <-> 右下）
            dx1 = -1; dy1 = -1; dx2 = 1; dy2 = 1; break;
        default:  // 45°（左下 <-> 右上）
            dx1 = -1; dy1 = 1; dx2 = 1; dy2 = -1; break;
    }
}

// 行地址统一走带符号的 ptrdiff_t：EO 的邻居 dy 可以是 -1，若写成
// static_cast<size_t>(dy) * stride 会把负数变成巨大无符号数（实现相关且有一堆
// -Wsign-conversion 告警）。有符号偏移在指针运算里语义明确。
template <class P>
P* row_ptr(P* base, std::ptrdiff_t y, int stride) {
    return base + y * static_cast<std::ptrdiff_t>(stride);
}

inline std::uint8_t clip8(int v) {
    return static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

inline int sign_of(int v) { return (v > 0) - (v < 0); }

// 平面几何：c_idx 0 = 亮度；1/2 = 4:2:0 色度。
struct PlaneGeom {
    int w = 0;
    int h = 0;
};

PlaneGeom plane_geom(int width, int height, int c_idx) {
    return c_idx == 0 ? PlaneGeom{width, height} : PlaneGeom{width / 2, height / 2};
}

}  // namespace

void sao_derive_offsets(SaoCtbParams& p, int log2_scale_luma, int log2_scale_chroma) {
    derive_offsets_impl(p);
    for (int c = 0; c < 3; ++c) {
        const int scale = (c == 0) ? log2_scale_luma : log2_scale_chroma;
        if (scale <= 0) continue;
        for (int i = 1; i < 5; ++i) p.offset_val[c][i] *= (1 << scale);
    }
}

void put_sao_params(std::vector<Bin>& out, const SaoCtbParams& p, bool slice_luma,
                    bool slice_chroma, bool has_left, bool has_up, bool merge_left,
                    bool merge_up) {
    if (!slice_luma && !slice_chroma) return;
    if (has_left) out.push_back({kCtxSaoMergeFlag, merge_left ? 1 : 0});
    if (has_up && !merge_left) out.push_back({kCtxSaoMergeFlag, merge_up ? 1 : 0});
    if (merge_left || merge_up) return;  // 整 CTB 继承邻居参数，后续语法不写

    const bool flag[3] = {slice_luma, slice_chroma, slice_chroma};
    for (int c = 0; c < 3; ++c) {
        if (!flag[c]) continue;
        if (c != 2) put_type_idx(out, p.type_idx[c]);
        if (p.type_idx[c] == kSaoNotApplied) continue;
        for (int i = 0; i < 4; ++i) put_offset_abs(out, p.offset_abs[c][i]);
        if (p.type_idx[c] == kSaoBand) {
            for (int i = 0; i < 4; ++i) {
                if (p.offset_abs[c][i] != 0)
                    out.push_back({kBypassCtx, p.offset_sign[c][i] ? 1 : 0});
            }
            put_band_position(out, p.band_position[c]);
        } else if (c != 2) {
            put_eo_class(out, p.eo_class[c]);
        }
    }
}

bool get_sao_params(BinCursor& c, bool slice_luma, bool slice_chroma, bool has_left,
                    bool has_up, const SaoCtbParams* left, const SaoCtbParams* up,
                    SaoCtbParams* out, bool* merge_left, bool* merge_up) {
    *out = SaoCtbParams{};
    bool ml = false, mu = false;
    if (slice_luma || slice_chroma) {
        if (has_left) {
            int b = 0;
            if (!c.decision(kCtxSaoMergeFlag, b)) return false;
            ml = (b != 0);
        }
        if (has_up && !ml) {
            int b = 0;
            if (!c.decision(kCtxSaoMergeFlag, b)) return false;
            mu = (b != 0);
        }
    }
    if (merge_left) *merge_left = ml;
    if (merge_up) *merge_up = mu;

    if (ml || mu) {
        const SaoCtbParams* src = ml ? left : up;
        if (!src) return false;
        *out = *src;  // SET_SAO：逐元素继承邻居
        derive_offsets_impl(*out);
        return true;
    }

    const bool flag[3] = {slice_luma, slice_chroma, slice_chroma};
    for (int ci = 0; ci < 3; ++ci) {
        if (!flag[ci]) {
            out->type_idx[ci] = kSaoNotApplied;
            continue;
        }
        if (ci == 2) {
            out->type_idx[2] = out->type_idx[1];
            out->eo_class[2] = out->eo_class[1];
        } else if (!get_type_idx(c, out->type_idx[ci])) {
            return false;
        }
        if (out->type_idx[ci] == kSaoNotApplied) continue;

        for (int i = 0; i < 4; ++i) {
            if (!get_offset_abs(c, out->offset_abs[ci][i])) return false;
        }
        if (out->type_idx[ci] == kSaoBand) {
            for (int i = 0; i < 4; ++i) {
                if (out->offset_abs[ci][i] != 0) {
                    int b = 0;
                    if (!c.bypass(b)) return false;
                    out->offset_sign[ci][i] = b;
                } else {
                    out->offset_sign[ci][i] = 0;
                }
            }
            if (!get_band_position(c, out->band_position[ci])) return false;
        } else if (ci != 2) {
            if (!get_eo_class(c, out->eo_class[ci])) return false;
        }
    }
    derive_offsets_impl(*out);
    return true;
}

void sao_filter_ctb(const SaoCtbParams& p, int c_idx, int x0, int y0, int ctb_w,
                    int ctb_h, const std::uint8_t* src, int src_stride,
                    std::uint8_t* dst, int dst_stride, int pic_w, int pic_h,
                    int dst_x0, int dst_y0) {
    if (x0 < 0 || y0 < 0 || x0 >= pic_w || y0 >= pic_h) return;
    const int x1 = std::min(x0 + ctb_w, pic_w);
    const int y1 = std::min(y0 + ctb_h, pic_h);
    const int type = p.type_idx[c_idx];

    if (type == kSaoNotApplied) {
        for (int y = y0; y < y1; ++y) {
            const std::uint8_t* s = row_ptr(src, y, src_stride) + x0;
            std::uint8_t* d = row_ptr(dst, y - dst_y0, dst_stride);
            std::copy(s, s + (x1 - x0), d + (x0 - dst_x0));
        }
        return;
    }

    if (type == kSaoBand) {
        int table[32] = {0};
        for (int k = 0; k < 4; ++k)
            table[(k + p.band_position[c_idx]) & 31] = p.offset_val[c_idx][k + 1];
        for (int y = y0; y < y1; ++y) {
            const std::uint8_t* s = row_ptr(src, y, src_stride);
            std::uint8_t* d = row_ptr(dst, y - dst_y0, dst_stride);
            for (int x = x0; x < x1; ++x) {
                const int v = s[x];
                d[x - dst_x0] = clip8(v + table[(v >> 3) & 31]);  // 8bit：BitDepth-5 = 3
            }
        }
        return;
    }

    // EDGE：两个邻居任一越界则该样本不修改（8.7.3.4 画面边界规则）。
    int dx1, dy1, dx2, dy2;
    eo_neighbours(p.eo_class[c_idx], dx1, dy1, dx2, dy2);
    const int* off = p.offset_val[c_idx];
    const int bx0 = std::max(x0, -dx1), bx1 = std::min(x1, pic_w - dx1);
    const int by0 = std::max(y0, -dy1), by1 = std::min(y1, pic_h - dy1);
    const int cx0 = std::max(x0, -dx2), cx1 = std::min(x1, pic_w - dx2);
    const int cy0 = std::max(y0, -dy2), cy1 = std::min(y1, pic_h - dy2);

    for (int y = y0; y < y1; ++y) {
        const std::uint8_t* srow = row_ptr(src, y, src_stride);
        std::uint8_t* drow = row_ptr(dst, y - dst_y0, dst_stride);
        for (int x = x0; x < x1; ++x) drow[x - dst_x0] = srow[x];  // 默认原样
        if (y < by0 || y >= by1 || y < cy0 || y >= cy1) continue;
        const std::uint8_t* n1row = row_ptr(srow, dy1, src_stride);
        const std::uint8_t* n2row = row_ptr(srow, dy2, src_stride);
        const int lo = std::max(bx0, cx0);
        const int hi = std::min(bx1, cx1);
        for (int x = lo; x < hi; ++x) {
            const int v = srow[x];
            const int d0 = sign_of(v - n1row[x + dx1]);
            const int d1 = sign_of(v - n2row[x + dx2]);
            const int o = off[kEdgeIdx[2 + d0 + d1]];
            if (o != 0) drow[x - dst_x0] = clip8(v + o);
        }
    }
}

// ── 编码端参数决策 ────────────────────────────────────────────────────────
namespace {

constexpr int kMaxCtb = 64;

struct PlaneStats {
    // 32 个 band 的 (count, Σ(src-rec), Σ(src-rec)²)
    std::int64_t band_cnt[32] = {};
    std::int64_t band_sum[32] = {};
    std::int64_t band_sq[32] = {};
    // 4 个 EO 方向 × 5 个 edgeIdx 类别
    std::int64_t eo_cnt[4][5] = {};
    std::int64_t eo_sum[4][5] = {};
    std::int64_t eo_sq[4][5] = {};
    std::int64_t ssd = 0;  // Σ(src-rec)²，即「不启用」的失真
};

PlaneStats collect_stats(const SaoFrameInput& in, int c_idx, int x0, int y0, int w,
                         int h) {
    PlaneStats st;
    const std::uint8_t* src = in.src[c_idx];
    const std::uint8_t* rec = in.rec[c_idx];
    const int ss = in.stride[c_idx];
    const PlaneGeom geom = plane_geom(in.width, in.height, c_idx);
    const int x1 = std::min(x0 + w, geom.w);
    const int y1 = std::min(y0 + h, geom.h);

    for (int y = y0; y < y1; ++y) {
        const std::uint8_t* s = row_ptr(src, y, ss);
        const std::uint8_t* r = row_ptr(rec, y, ss);
        for (int x = x0; x < x1; ++x) {
            const int d = static_cast<int>(s[x]) - static_cast<int>(r[x]);
            const int b = (r[x] >> 3) & 31;
            st.band_cnt[b] += 1;
            st.band_sum[b] += d;
            st.band_sq[b] += static_cast<std::int64_t>(d) * d;
            st.ssd += static_cast<std::int64_t>(d) * d;
        }
    }

    // EO 统计：只有两个邻居都在画面内的样本才会被滤波。
    for (int cls = 0; cls < 4; ++cls) {
        int dx1, dy1, dx2, dy2;
        eo_neighbours(cls, dx1, dy1, dx2, dy2);
        const int bx0 = std::max(x0, -dx1), bx1 = std::min(x1, geom.w - dx1);
        const int by0 = std::max(y0, -dy1), by1 = std::min(y1, geom.h - dy1);
        const int cx0 = std::max(x0, -dx2), cx1 = std::min(x1, geom.w - dx2);
        const int cy0 = std::max(y0, -dy2), cy1 = std::min(y1, geom.h - dy2);
        const int loy = std::max(by0, cy0), hiy = std::min(by1, cy1);
        const int lox = std::max(bx0, cx0), hix = std::min(bx1, cx1);
        for (int y = loy; y < hiy; ++y) {
            const std::uint8_t* r = row_ptr(rec, y, ss);
            const std::uint8_t* s = row_ptr(src, y, ss);
            const std::uint8_t* n1 = row_ptr(r, dy1, ss);
            const std::uint8_t* n2 = row_ptr(r, dy2, ss);
            for (int x = lox; x < hix; ++x) {
                const int v = r[x];
                const int cat =
                    2 + sign_of(v - n1[x + dx1]) + sign_of(v - n2[x + dx2]);
                const int d = static_cast<int>(s[x]) - v;
                st.eo_cnt[cls][cat] += 1;
                st.eo_sum[cls][cat] += d;
                st.eo_sq[cls][cat] += static_cast<std::int64_t>(d) * d;
            }
        }
    }
    return st;
}

int clamp_int(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// 由 (count, sum) 求最优整数 offset（四舍五入后夹到给定区间）。
int best_offset(std::int64_t cnt, std::int64_t sum, int lo, int hi) {
    if (cnt <= 0) return 0;
    const double mean = static_cast<double>(sum) / static_cast<double>(cnt);
    return clamp_int(static_cast<int>(std::lround(mean)), lo, hi);
}

int estimate_plane_bits(const SaoCtbParams& p, int c_idx) {
    int bits = 0;
    // c_idx==2 的 type_idx 由 c_idx==1 继承，不写 type 语法。
    if (c_idx != 2) {
        bits = 1;  // sao_type_idx 的 ctx bin
        if (p.type_idx[c_idx] == kSaoNotApplied) return bits;
        bits += 1;  // type 的 bypass bin
    } else {
        if (p.type_idx[c_idx] == kSaoNotApplied) return 0;
    }
    for (int i = 0; i < 4; ++i) {
        const int a = p.offset_abs[c_idx][i];
        bits += (a >= kOffsetAbsMax) ? kOffsetAbsMax : (a + 1);
    }
    if (p.type_idx[c_idx] == kSaoBand) {
        bits += 5;
        for (int i = 0; i < 4; ++i)
            if (p.offset_abs[c_idx][i] != 0) bits += 1;
    } else if (c_idx != 2) {
        bits += 2;  // sao_eo_class
    }
    return bits;
}

// 真实 SSE：跑一遍滤波再比对源（含裁剪与画面边界规则）。
std::int64_t exact_sse(const SaoFrameInput& in, const SaoCtbParams& p, int c_idx,
                       int x0, int y0, int w, int h) {
    const PlaneGeom geom = plane_geom(in.width, in.height, c_idx);
    const int x1 = std::min(x0 + w, geom.w);
    const int y1 = std::min(y0 + h, geom.h);
    // dst 用「局部」原点到 (x0,y0)，写进一块 CTB 大小的临时缓冲；src 仍是整帧
    // 绝对坐标，故跨 CTB 邻居照常可读（8.7.3.4 要求用 SAO 前的相邻样本）。
    const int bw = x1 - x0;
    static thread_local std::uint8_t tmp[kMaxCtb * kMaxCtb];
    sao_filter_ctb(p, c_idx, x0, y0, w, h, in.rec[c_idx], in.stride[c_idx], tmp, bw,
                   geom.w, geom.h, x0, y0);
    std::int64_t sse = 0;
    for (int y = y0; y < y1; ++y) {
        const std::uint8_t* s = row_ptr(in.src[c_idx], y, in.stride[c_idx]);
        const std::uint8_t* d = row_ptr(tmp, y - y0, bw);
        for (int x = x0; x < x1; ++x) {
            const int e = static_cast<int>(s[x]) - static_cast<int>(d[x - x0]);
            sse += static_cast<std::int64_t>(e) * e;
        }
    }
    return sse;
}

// 单平面候选（BO 或 EO）：结构 + 4 个 offset_abs/sign。
struct PlaneCand {
    int type = kSaoNotApplied;
    int band_position = 0;
    int eo_class = 0;
    int abs[4] = {0, 0, 0, 0};
    int sign[4] = {0, 0, 0, 0};
};

void store_cand(const PlaneCand& c, int c_idx, SaoCtbParams* p) {
    p->type_idx[c_idx] = c.type;
    p->band_position[c_idx] = c.band_position;
    p->eo_class[c_idx] = c.eo_class;
    for (int i = 0; i < 4; ++i) {
        p->offset_abs[c_idx][i] = c.abs[i];
        p->offset_sign[c_idx][i] = c.sign[i];
    }
    // 立刻展开成 offset_val：后面所有 exact_sse 都靠它滤波，漏了这一步候选的
    // 失真会恒等于「不启用」，RD 永远选不到 SAO。
    derive_offsets_impl(*p);
}

// BO：在 32 个 band 起点里按整帧代价选最优。
// 只累计「被这 4 个 band 覆盖」的 sq-2o·sum+o²·cnt 会把其余 28 个 band 的失真
// 丢掉：空 band 起点白拿 sse=0 + 最少 bit，永远胜过真正有增益的起点（曾导致
// 均匀 -4 偏置的画面 RD 选中全 0 offset，SAO 静默空转）。正确口径是
// 「整帧 SSD − 这 4 个 band 的改善量」，各 band_position 才彼此可比。
PlaneCand pick_band(const PlaneStats& st, double lambda, int c_idx) {
    PlaneCand best;
    double best_j = 0;
    bool first = true;
    for (int bp = 0; bp < 32; ++bp) {
        PlaneCand c;
        c.type = kSaoBand;
        c.band_position = bp;
        std::int64_t gain = 0;
        for (int k = 0; k < 4; ++k) {
            const int b = (bp + k) & 31;
            const int o =
                best_offset(st.band_cnt[b], st.band_sum[b], -kOffsetAbsMax, kOffsetAbsMax);
            gain += 2 * static_cast<std::int64_t>(o) * st.band_sum[b] -
                    static_cast<std::int64_t>(o) * o * st.band_cnt[b];
            c.abs[k] = o < 0 ? -o : o;
            c.sign[k] = o < 0 ? 1 : 0;
        }
        SaoCtbParams tmp;
        store_cand(c, c_idx, &tmp);
        const double j = static_cast<double>(
                             std::max<std::int64_t>(st.ssd - gain, 0)) +
                         lambda * estimate_plane_bits(tmp, c_idx);
        if (first || j < best_j) {
            first = false;
            best_j = j;
            best = c;
        }
    }
    return best;
}

// EO：cat 0/1 只能取正 offset，cat 3/4 只能取负；类别 2 恒为 0，不写 bit。
PlaneCand pick_edge(const PlaneStats& st, int cls) {
    PlaneCand c;
    c.type = kSaoEdge;
    c.eo_class = cls;
    for (int cat = 0; cat < 5; ++cat) {
        const bool positive = (cat == 0 || cat == 1);
        const int lo = positive ? 0 : -kOffsetAbsMax;
        const int hi = positive ? kOffsetAbsMax : 0;
        const int o = best_offset(st.eo_cnt[cls][cat], st.eo_sum[cls][cat], lo, hi);
        // cat0 → abs0（正）；cat1 → abs1（正）；cat3 → abs2（负）；cat4 → abs3（负）。
        if (cat == 0) c.abs[0] = o;
        else if (cat == 1) c.abs[1] = o;
        else if (cat == 3) c.abs[2] = -o;
        else if (cat == 4) c.abs[3] = -o;
    }
    return c;
}

// 亮度：不启用 / BO / EO 三者按真实 SSE + λ·bits 选优。
void choose_luma(const SaoFrameInput& in, const PlaneStats& st, double lambda, int x0,
                 int y0, int w, int h, SaoCtbParams* out) {
    out->type_idx[0] = kSaoNotApplied;
    double best_j = static_cast<double>(exact_sse(in, *out, 0, x0, y0, w, h)) + lambda * 1.0;

    PlaneCand band = pick_band(st, lambda, 0);
    SaoCtbParams p = *out;
    store_cand(band, 0, &p);
    const double band_j = static_cast<double>(exact_sse(in, p, 0, x0, y0, w, h)) +
                          lambda * estimate_plane_bits(p, 0);

    PlaneCand edge;
    double edge_j = 1e300;
    for (int cls = 0; cls < 4; ++cls) {
        const PlaneCand e = pick_edge(st, cls);
        SaoCtbParams q = *out;
        store_cand(e, 0, &q);
        const double j = static_cast<double>(exact_sse(in, q, 0, x0, y0, w, h)) +
                         lambda * estimate_plane_bits(q, 0);
        if (j < edge_j) {
            edge_j = j;
            edge = e;
        }
    }

    if (band_j < best_j && band_j <= edge_j) {
        store_cand(band, 0, out);
    } else if (edge_j < best_j) {
        store_cand(edge, 0, out);
    }
}

// 色度：type_idx / eo_class 必须 U、V 共享，band_position 与 offset 各自独立。
void choose_chroma(const SaoFrameInput& in, const PlaneStats& su, const PlaneStats& sv,
                   double lambda, int x0, int y0, int w, int h, SaoCtbParams* out) {
    out->type_idx[1] = out->type_idx[2] = kSaoNotApplied;
    const auto cost = [&](const SaoCtbParams& p) {
        return static_cast<double>(exact_sse(in, p, 1, x0, y0, w, h)) +
               static_cast<double>(exact_sse(in, p, 2, x0, y0, w, h)) +
               lambda * (estimate_plane_bits(p, 1) + estimate_plane_bits(p, 2));
    };
    double best_j = cost(*out);

    // BAND：两个平面的 band 起点各自最优。
    {
        SaoCtbParams p = *out;
        const PlaneCand cu = pick_band(su, lambda, 1);
        const PlaneCand cv = pick_band(sv, lambda, 2);
        store_cand(cu, 1, &p);
        store_cand(cv, 2, &p);
        const double j = cost(p);
        if (j < best_j) {
            best_j = j;
            *out = p;
        }
    }

    // EDGE：共享 eo_class，逐平面单独取 offset。
    for (int cls = 0; cls < 4; ++cls) {
        SaoCtbParams p = *out;
        const PlaneCand cu = pick_edge(su, cls);
        const PlaneCand cv = pick_edge(sv, cls);
        store_cand(cu, 1, &p);
        store_cand(cv, 2, &p);
        p.eo_class[2] = p.eo_class[1] = cls;
        const double j = cost(p);
        if (j < best_j) {
            best_j = j;
            *out = p;
        }
    }
}

// 用真实滤波评估整 CTB 的失真（merge 候选与自有参数都用它比较）。
std::int64_t ctb_sse(const SaoFrameInput& in, const SaoCtbParams& p, int x0, int y0,
                     int w, int h) {
    return exact_sse(in, p, 0, x0, y0, w, h) +
           exact_sse(in, p, 1, x0 / 2, y0 / 2, w / 2, h / 2) +
           exact_sse(in, p, 2, x0 / 2, y0 / 2, w / 2, h / 2);
}

}  // namespace

void compute_sao_table(const SaoFrameInput& in, double lambda_luma,
                       double lambda_chroma, SaoTable* table) {
    const int ctb_w = (in.width + kMaxCtb - 1) / kMaxCtb;
    const int ctb_h = (in.height + kMaxCtb - 1) / kMaxCtb;
    table->slice_luma = true;
    table->slice_chroma = true;
    table->ctb_w = ctb_w;
    table->ctb_h = ctb_h;
    const std::size_t n = static_cast<std::size_t>(ctb_w) * static_cast<std::size_t>(ctb_h);
    table->params.assign(n, SaoCtbParams{});
    table->merge_left.assign(n, 0);
    table->merge_up.assign(n, 0);

    for (int cy = 0; cy < ctb_h; ++cy) {
        for (int cx = 0; cx < ctb_w; ++cx) {
            const int x0 = cx * kMaxCtb;
            const int y0 = cy * kMaxCtb;
            const int w = std::min(kMaxCtb, in.width - x0);
            const int h = std::min(kMaxCtb, in.height - y0);
            const std::size_t idx = static_cast<std::size_t>(cy) *
                                        static_cast<std::size_t>(ctb_w) +
                                    static_cast<std::size_t>(cx);

            SaoCtbParams own;
            choose_luma(in, collect_stats(in, 0, x0, y0, w, h), lambda_luma, x0, y0, w, h,
                        &own);
            if (table->slice_chroma) {
                choose_chroma(in, collect_stats(in, 1, x0 / 2, y0 / 2, w / 2, h / 2),
                              collect_stats(in, 2, x0 / 2, y0 / 2, w / 2, h / 2),
                              lambda_chroma, x0 / 2, y0 / 2, w / 2, h / 2, &own);
            }
            derive_offsets_impl(own);

            const int has_left = cx > 0;
            const int has_up = cy > 0;
            // 不 merge 时仍要写 0 标志，代价照算。
            double own_j = static_cast<double>(ctb_sse(in, own, x0, y0, w, h)) +
                           lambda_luma * estimate_plane_bits(own, 0);
            if (table->slice_chroma) {
                own_j += lambda_chroma * (estimate_plane_bits(own, 1) +
                                          estimate_plane_bits(own, 2));
            }
            own_j += lambda_luma * (has_left + has_up);

            int best_kind = 0;  // 0 = 自有参数，1 = 左，2 = 上
            double best_j = own_j;
            const SaoCtbParams* nb_left = has_left ? &table->params[idx - 1] : nullptr;
            const SaoCtbParams* nb_up =
                has_up ? &table->params[idx - static_cast<std::size_t>(ctb_w)] : nullptr;

            if (nb_left) {
                const double j = static_cast<double>(ctb_sse(in, *nb_left, x0, y0, w, h)) +
                                 lambda_luma * 1.0;
                if (j < best_j) {
                    best_j = j;
                    best_kind = 1;
                }
            }
            if (nb_up) {
                const double j = static_cast<double>(ctb_sse(in, *nb_up, x0, y0, w, h)) +
                                 lambda_luma * (has_left ? 2.0 : 1.0);
                if (j < best_j) {
                    best_j = j;
                    best_kind = 2;
                }
            }

            table->params[idx] = best_kind == 0 ? own : (best_kind == 1 ? *nb_left : *nb_up);
            table->merge_left[idx] = static_cast<std::uint8_t>(best_kind == 1);
            table->merge_up[idx] = static_cast<std::uint8_t>(best_kind == 2);
#if defined(TERMCHART_HEVC_SAO_DUMP)  // 诊断专用：逐 CTB 打印决策结果到 stderr
            if (std::getenv("TERMCHART_HEVC_SAO_DUMP")) {
                const SaoCtbParams& q = table->params[idx];
                std::fprintf(stderr,
                             "ctb(%d,%d) merge=%d/%d Y(t=%d cls=%d bp=%d o=%d,%d,%d,%d) "
                             "U(t=%d cls=%d o=%d,%d,%d,%d) V(t=%d bp=%d o=%d,%d,%d,%d)\n",
                             cx, cy, table->merge_left[idx], table->merge_up[idx],
                             q.type_idx[0], q.eo_class[0], q.band_position[0],
                             q.offset_val[0][1], q.offset_val[0][2], q.offset_val[0][3],
                             q.offset_val[0][4], q.type_idx[1], q.eo_class[1],
                             q.offset_val[1][1], q.offset_val[1][2], q.offset_val[1][3],
                             q.offset_val[1][4], q.type_idx[2], q.band_position[2],
                             q.offset_val[2][1], q.offset_val[2][2], q.offset_val[2][3],
                             q.offset_val[2][4]);
            }
#endif
        }
    }
}

void apply_sao_table(const SaoFrameInput& in, const SaoTable& table,
                     std::uint8_t* const dst[3], const int dst_stride[3]) {
    for (int cy = 0; cy < table.ctb_h; ++cy) {
        for (int cx = 0; cx < table.ctb_w; ++cx) {
            const int x0 = cx * kMaxCtb;
            const int y0 = cy * kMaxCtb;
            const int w = std::min(kMaxCtb, in.width - x0);
            const int h = std::min(kMaxCtb, in.height - y0);
            const std::size_t idx = static_cast<std::size_t>(cy) *
                                        static_cast<std::size_t>(table.ctb_w) +
                                    static_cast<std::size_t>(cx);
            const SaoCtbParams& p = table.params[idx];
            const PlaneGeom gy = plane_geom(in.width, in.height, 0);
            sao_filter_ctb(p, 0, x0, y0, w, h, in.rec[0], in.stride[0], dst[0],
                           dst_stride[0], gy.w, gy.h);
            const PlaneGeom gc = plane_geom(in.width, in.height, 1);
            sao_filter_ctb(p, 1, x0 / 2, y0 / 2, w / 2, h / 2, in.rec[1], in.stride[1],
                           dst[1], dst_stride[1], gc.w, gc.h);
            sao_filter_ctb(p, 2, x0 / 2, y0 / 2, w / 2, h / 2, in.rec[2], in.stride[2],
                           dst[2], dst_stride[2], gc.w, gc.h);
        }
    }
}
}  // namespace termchart::image::codec::hevc
