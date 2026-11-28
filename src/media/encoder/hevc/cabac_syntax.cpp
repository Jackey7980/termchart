// cabac_syntax.cpp —— HEVC CABAC 语法元素映射
#include "cabac_syntax.hpp"

#include "cabac_context.hpp"
#include "cabac_engine.hpp"

namespace termchart::image::codec::hevc {

// ── BinCursor ────────────────────────────────────────────────────────────
bool BinCursor::decision(int expect_ctx, int& val) {
    if (pos_ >= count_) return false;
    const Bin& b = bins_[pos_];
    if (b.ctx != expect_ctx) return false;
    val = b.val;
    ++pos_;
    return true;
}

bool BinCursor::bypass(int& val) {
    if (pos_ >= count_) return false;
    const Bin& b = bins_[pos_];
    if (b.ctx != kBypassCtx) return false;
    val = b.val;
    ++pos_;
    return true;
}

// ── I_PCM 档（区间反解引擎）──────────────────────────────────────────────
int split_cu_flag_ctx(int x0, int y0) {
    // 仅 CTB 根（64x64）用 (ctb_x>0)+(ctb_y>0)；非根 CU 的 CTB 内左上对角
    // 子块 ctxInc=1、内部子块 ctxInc=2。用 CTB 内坐标会在第 2 个及以后的
    // CTB 左/上边界漏掉邻居，导致右半帧全错，故统一用绝对坐标。
    return (x0 > 0 ? 1 : 0) + (y0 > 0 ? 1 : 0);
}

void write_split_cu_flag(CabacEncoder& enc, int x0, int y0, int bin) {
    enc.encode_bin(split_cu_flag_ctx(x0, y0), bin);
}

void write_part_mode_2nx2n(CabacEncoder& enc) {
    // FFmpeg ff_hevc_part_mode_decode(): if (GET_CABAC(PART_MODE_OFFSET))
    // return PART_2Nx2N，即 bin "1" → PART_2Nx2N，只读 1 个 bin。
    enc.encode_bin(kPartModeCtx, 1);
}

void write_pcm_flag(CabacEncoder& enc, int bit) {
    // pcm_flag 是【终止 bin】：FFmpeg ff_hevc_pcm_flag_decode() 直接返回
    // get_cabac_terminate(&lc->cc)，不占用上下文（PCM_FLAG 的 NUM_BINS=0）。
    enc.encode_terminating(bit);
}

void write_end_of_slice_segment_flag(CabacEncoder& enc, int bit) {
    // HEVC 7.3.8.1：每个 CTB 之后读一次终止 bin；除最后一个 CTB 外必须为 0。
    enc.encode_terminating(bit);
}

// ── 真压缩档：put_* 与 FFmpeg libavcodec/hevc/cabac.c 的 *_decode 对偶 ──
void put_split_cu_flag(std::vector<Bin>& out, int ctx_inc, int value) {
    out.push_back({kCtxSplitCuFlag + ctx_inc, value});
}

void put_part_mode_intra(std::vector<Bin>& out, bool part_2nx2n) {
    // ff_hevc_part_mode_decode：intra 且 log2CbSize==MinCbLog2SizeY 时
    // 首 bin=1 → PART_2Nx2N，首 bin=0 → PART_NxN（只读 1 个 bin）。
    out.push_back({kCtxPartMode, part_2nx2n ? 1 : 0});
}

void put_prev_intra_luma_pred_flag(std::vector<Bin>& out, int value) {
    out.push_back({kCtxPrevIntraLumaPred, value});
}

void put_mpm_idx(std::vector<Bin>& out, int idx) {
    // ff_hevc_mpm_idx_decode: while (i < 2 && get_cabac_bypass()) i++;
    for (int i = 0; i < idx; ++i) out.push_back({kBypassCtx, 1});
    if (idx < 2) out.push_back({kBypassCtx, 0});
}

void put_rem_intra_luma_pred_mode(std::vector<Bin>& out, int mode) {
    // ff_hevc_rem_intra_luma_pred_mode_decode: 5 个 bypass bit，MSB first。
    for (int i = 4; i >= 0; --i) out.push_back({kBypassCtx, (mode >> i) & 1});
}

void put_intra_chroma_pred_mode(std::vector<Bin>& out, int mode) {
    // ff_hevc_intra_chroma_pred_mode_decode: GET_CABAC(18) 为 0 时返回 4；
    // 为 1 时再读 2 个 bypass bit 得到 0..3。
    if (mode == 4) {
        out.push_back({kCtxIntraChromaPred, 0});
    } else {
        out.push_back({kCtxIntraChromaPred, 1});
        out.push_back({kBypassCtx, (mode >> 1) & 1});
        out.push_back({kBypassCtx, mode & 1});
    }
}

void put_cbf_luma(std::vector<Bin>& out, int trafo_depth, int value) {
    // 解码端为 GET_CABAC(CBF_LUMA_OFFSET + !trafo_depth)
    // → 根（trafoDepth==0）用 ctx 41，子层（trafoDepth>0）用 ctx 40。
    // 必须与 get_cbf_luma 同式：两侧 ctx 不一致会让算术状态自该 bin 起与解码端分叉。
    out.push_back({kCtxCbfLuma + (trafo_depth ? 0 : 1), value});
}

void put_cbf_cb_cr(std::vector<Bin>& out, int c_idx, int trafo_depth, int value) {
    // 解码端为 GET_CABAC(CBF_CB_CR_OFFSET + trafo_depth)：cb 与 cr 共用同一 ctxInc，
    // 且只随 trafoDepth 变化（该语法元素共 5 个 ctx，正好覆盖 trafoDepth 0..4）。
    (void)c_idx;
    out.push_back({kCtxCbfCbCr + trafo_depth, value});
}

void put_split_transform_flag(std::vector<Bin>& out, int log2_trafo_size, int value) {
    // ff_hevc_split_transform_flag_decode: ctx = 37 + 5 - log2_trafo_size
    out.push_back({kCtxSplitTransform + 5 - log2_trafo_size, value});
}

void put_transform_skip_flag(std::vector<Bin>& out, int c_idx, int value) {
    // hevc_transform_skip_flag_decode: GET_CABAC(TRANSFORM_SKIP_FLAG_OFFSET + !!c_idx)
    out.push_back({kCtxTransformSkip + (c_idx ? 1 : 0), value});
}

bool get_transform_skip_flag(BinCursor& c, int c_idx, int& value) {
    return c.decision(kCtxTransformSkip + (c_idx ? 1 : 0), value);
}

bool get_split_cu_flag(BinCursor& c, int ctx_inc, int& value) {
    return c.decision(kCtxSplitCuFlag + ctx_inc, value);
}

bool get_part_mode_intra(BinCursor& c, bool& part_2nx2n) {
    int v = 0;
    if (!c.decision(kCtxPartMode, v)) return false;
    part_2nx2n = (v != 0);
    return true;
}

bool get_prev_intra_luma_pred_flag(BinCursor& c, int& value) {
    return c.decision(kCtxPrevIntraLumaPred, value);
}

bool get_mpm_idx(BinCursor& c, int& idx) {
    idx = 0;
    while (idx < 2) {
        int v = 0;
        if (!c.bypass(v)) return false;
        if (!v) break;
        ++idx;
    }
    return true;
}

bool get_rem_intra_luma_pred_mode(BinCursor& c, int& mode) {
    mode = 0;
    for (int i = 0; i < 5; ++i) {
        int v = 0;
        if (!c.bypass(v)) return false;
        mode = (mode << 1) | v;
    }
    return true;
}

bool get_intra_chroma_pred_mode(BinCursor& c, int& mode) {
    int v = 0;
    if (!c.decision(kCtxIntraChromaPred, v)) return false;
    if (!v) { mode = 4; return true; }
    int hi = 0, lo = 0;
    if (!c.bypass(hi) || !c.bypass(lo)) return false;
    mode = (hi << 1) | lo;
    return true;
}

bool get_cbf_luma(BinCursor& c, int trafo_depth, int& value) {
    return c.decision(kCtxCbfLuma + (trafo_depth ? 0 : 1), value);
}

bool get_cbf_cb_cr(BinCursor& c, int c_idx, int trafo_depth, int& value) {
    (void)c_idx;
    return c.decision(kCtxCbfCbCr + trafo_depth, value);
}

bool get_split_transform_flag(BinCursor& c, int log2_trafo_size, int& value) {
    return c.decision(kCtxSplitTransform + 5 - log2_trafo_size, value);
}

// ── P 片帧间语法元素 ─────────────────────────────────────────────────────
// 顺序与上下文逐条对齐 ffmpeg n8.1.2：hevcdec.c hls_coding_unit / hls_prediction_unit
// 与 cabac.c 的各 *_decode()。任何一个 ctx 或先后次序写错，解码端算术状态会从
// 该 bin 起整体分叉（表现是不报错但整片像素为垃圾）。
void put_skip_flag(std::vector<Bin>& out, int ctx_inc, int value) {
    // ff_hevc_skip_flag_decode(CABAC_ELEMS + SKIP_FLAG_OFFSET + inc)，inc∈0..2
    out.push_back({kCtxSkipFlag + ctx_inc, value});
}

bool get_skip_flag(BinCursor& c, int ctx_inc, int& value) {
    return c.decision(kCtxSkipFlag + ctx_inc, value);
}

void put_pred_mode_flag(std::vector<Bin>& out, int value) {
    // ff_hevc_pred_mode_decode 直接返回该 bin。ffmpeg 枚举 MODE_INTER=0 /
    // MODE_INTRA=1，HEVC 8.3.1：pred_mode_flag=0 ⇒ INTER，=1 ⇒ INTRA。
    out.push_back({kCtxPredMode, value});
}

bool get_pred_mode_flag(BinCursor& c, int& value) {
    return c.decision(kCtxPredMode, value);
}

void put_part_mode_inter_2nx2n(std::vector<Bin>& out) {
    // ff_hevc_part_mode_decode：首 bin=1 → PART_2Nx2N；帧间在 MinCbLog2SizeY 档
    // 也只需这 1 个 bin（本项目 CU 恒为 8x8 = MinCb）。
    out.push_back({kCtxPartMode, 1});
}

bool get_part_mode_inter_2nx2n(BinCursor& c, bool& part_2nx2n) {
    int v = 0;
    if (!c.decision(kCtxPartMode, v)) return false;
    part_2nx2n = (v != 0);
    return true;
}

void put_merge_flag(std::vector<Bin>& out, int value) {
    out.push_back({kCtxMergeFlag, value});
}

bool get_merge_flag(BinCursor& c, int& value) {
    return c.decision(kCtxMergeFlag, value);
}

void put_merge_idx(std::vector<Bin>& out, int idx, int max_cand) {
    // ffmpeg ff_hevc_merge_idx_decode：首 bin 走 CABAC ctx（kCtxMergeIdx），
    // 若非 0 则后续用 get_cabac_bypass 递增，直到遇 0 或到 max_cand-1。
    //   idx=0            → ctx 0
    //   idx=k (1..max-2) → ctx 1, 再 k 个 bypass 1, 最后 1 个 bypass 0
    //   idx=max-1        → ctx 1, 再 (max-2) 个 bypass 1（无终止 0）
    if (max_cand <= 1) return;
    const int last = max_cand - 1;
    if (idx <= 0) {
        out.push_back({kCtxMergeIdx, 0});
        return;
    }
    out.push_back({kCtxMergeIdx, 1});
    if (idx < last) {
        for (int i = 1; i < idx; ++i) out.push_back({kBypassCtx, 1});
        out.push_back({kBypassCtx, 0});
    } else {
        for (int i = 1; i < last; ++i) out.push_back({kBypassCtx, 1});
    }
}

bool get_merge_idx(BinCursor& c, int max_cand, int& idx) {
    idx = 0;
    if (max_cand <= 1) return true;
    const int last = max_cand - 1;
    if (!c.decision(kCtxMergeIdx, idx)) return false;
    if (idx != 0) {
        while (idx < last) {
            int b = 0;
            if (!c.bypass(b)) return false;
            if (!b) break;
            ++idx;
        }
    }
    return true;
}

void put_rqt_root_cbf(std::vector<Bin>& out, int value) {
    // 解码端 rqt_root_cbf = ff_hevc_no_residual_syntax_flag_decode()（直接取值，
    // 无取反）：ctx 36，1 = 有残差 ⇒ 后面跟 transform_tree。
    out.push_back({kCtxNoResidualData, value});
}

bool get_rqt_root_cbf(BinCursor& c, int& value) {
    return c.decision(kCtxNoResidualData, value);
}

namespace {

// ffmpeg cabac.c mvd_decode() 的对偶：magnitude >= 2 时前缀 p 个 1 + 1 个 0，
// 再 p+1 个后缀 bit（MSB first），最后 1 个符号 bit（1 = 负）。
//   m ∈ [2^(p+1), 2^(p+2)-1]  ⇒  p = floor(log2(m)) - 1
void put_mvd_magnitude(std::vector<Bin>& out, int magnitude, bool negative) {
    int p = 0;
    int base = 2;  // = 2^(p+1)
    while (magnitude >= base * 2) {
        base *= 2;
        ++p;
    }
    for (int i = 0; i < p; ++i) out.push_back({kBypassCtx, 1});
    out.push_back({kBypassCtx, 0});
    const int suffix = magnitude - base;
    for (int i = p; i >= 0; --i) out.push_back({kBypassCtx, (suffix >> i) & 1});
    out.push_back({kBypassCtx, negative ? 1 : 0});
}

// 返回 false 表示 bin 流不足（仅用于自检，真实解码不会发生）。
bool get_mvd_magnitude(BinCursor& c, int& magnitude) {
    int k = 1;
    int ret = 2;
    while (k < 31) {
        int v = 0;
        if (!c.bypass(v)) return false;
        if (!v) break;
        ret += 1 << k;
        ++k;
    }
    for (int i = k - 1; i >= 0; --i) {
        int v = 0;
        if (!c.bypass(v)) return false;
        ret += v << i;
    }
    magnitude = ret;
    return true;
}

}  // namespace

void put_mvd_coding(std::vector<Bin>& out, int mvd_x, int mvd_y) {
    const int ax = mvd_x < 0 ? -mvd_x : mvd_x;
    const int ay = mvd_y < 0 ? -mvd_y : mvd_y;
    // ffmpeg hls_mvd_coding()：greater0(x)、greater0(y)、greater1(x)、
    // greater1(y)，随后整个分量 x、整个分量 y。
    out.push_back({kCtxMvdGreater0, ax > 0 ? 1 : 0});
    out.push_back({kCtxMvdGreater0, ay > 0 ? 1 : 0});
    if (ax > 0) out.push_back({kCtxMvdGreater1, ax > 1 ? 1 : 0});
    if (ay > 0) out.push_back({kCtxMvdGreater1, ay > 1 ? 1 : 0});
    if (ax == 1) out.push_back({kBypassCtx, mvd_x < 0 ? 1 : 0});
    else if (ax > 1) put_mvd_magnitude(out, ax, mvd_x < 0);
    if (ay == 1) out.push_back({kBypassCtx, mvd_y < 0 ? 1 : 0});
    else if (ay > 1) put_mvd_magnitude(out, ay, mvd_y < 0);
}

bool get_mvd_coding(BinCursor& c, int& mvd_x, int& mvd_y) {
    int gx = 0, gy = 0;
    if (!c.decision(kCtxMvdGreater0, gx)) return false;
    if (!c.decision(kCtxMvdGreater0, gy)) return false;
    int g1x = 0, g1y = 0;
    if (gx != 0 && !c.decision(kCtxMvdGreater1, g1x)) return false;
    if (gy != 0 && !c.decision(kCtxMvdGreater1, g1y)) return false;
    mvd_x = 0;
    mvd_y = 0;
    for (int pass = 0; pass < 2; ++pass) {
        const int g = pass == 0 ? gx : gy;
        const int g1 = pass == 0 ? g1x : g1y;
        int mag = 0;
        if (g != 0) {
            if (g1 != 0) {
                if (!get_mvd_magnitude(c, mag)) return false;
            } else {
                mag = 1;
            }
            int sign = 0;
            if (!c.bypass(sign)) return false;
            if (sign != 0) mag = -mag;
        }
        if (pass == 0) mvd_x = mag;
        else mvd_y = mag;
    }
    return true;
}

void put_mvp_lx_flag(std::vector<Bin>& out, int value) {
    out.push_back({kCtxMvpLxFlag, value});
}

bool get_mvp_lx_flag(BinCursor& c, int& value) {
    return c.decision(kCtxMvpLxFlag, value);
}

void put_ref_idx_l0(std::vector<Bin>& out, int idx, int nb_refs) {
    // ff_hevc_ref_idx_lx_decode：前 min(max,2) 个 bin 走 ctx，其后走 bypass。
    // 单参考（nb_refs==1, max==0）时解码端不读任何 bin。
    const int max = nb_refs - 1;
    if (max <= 0) return;
    const int max_ctx = max < 2 ? max : 2;
    for (int i = 0; i < max_ctx; ++i) {
        const bool more = idx > i;
        out.push_back({kCtxRefIdxL0 + i, more ? 1 : 0});
        if (!more) return;
    }
    for (int i = max_ctx; i < max; ++i) {
        const bool more = idx > i;
        out.push_back({kBypassCtx, more ? 1 : 0});
        if (!more) return;
    }
}

namespace {

// ff_hevc_ref_idx_lx_decode：前 min(max,2) 个 bin 走 ctx，其后走 bypass；
// 全部为 1 时取最后一个参考。与 put_ref_idx_l0/l1 严格互逆。
bool ref_idx_lx_read(BinCursor& c, int base, int nb_refs, int& idx) {
    const int max = nb_refs - 1;
    idx = 0;
    if (max <= 0) return true;  // 单参考：解码端不读 ref_idx
    const int max_ctx = max < 2 ? max : 2;
    for (int i = 0; i < max_ctx; ++i) {
        int v = 0;
        if (!c.decision(base + i, v)) return false;
        if (!v) {
            idx = i;
            return true;
        }
    }
    for (int i = max_ctx; i < max; ++i) {
        int v = 0;
        if (!c.bypass(v)) return false;
        if (!v) {
            idx = i;
            return true;
        }
    }
    idx = max;
    return true;
}

}  // namespace

bool get_ref_idx_l0(BinCursor& c, int nb_refs, int& idx) {
    return ref_idx_lx_read(c, kCtxRefIdxL0, nb_refs, idx);
}

bool get_ref_idx_l1(BinCursor& c, int nb_refs, int& idx) {
    return ref_idx_lx_read(c, kCtxRefIdxL1, nb_refs, idx);
}

std::vector<Bin> merge_idx_bins(int idx, int max_cand) {
    std::vector<Bin> out;
    put_merge_idx(out, idx, max_cand);
    return out;
}

std::vector<Bin> mvd_coding_bins(int mvd_x, int mvd_y) {
    std::vector<Bin> out;
    put_mvd_coding(out, mvd_x, mvd_y);
    return out;
}

// ── CU 组合顺序 ──────────────────────────────────────────────────────────
std::vector<Bin> encode_cu_intra_bins(const CuIntraBins& p) {
    std::vector<Bin> out;
    if (p.emit_part_mode) put_part_mode_intra(out, p.part_2nx2n);
    put_prev_intra_luma_pred_flag(out, p.prev_intra_luma_pred_flag ? 1 : 0);
    if (p.prev_intra_luma_pred_flag) put_mpm_idx(out, p.mpm_idx);
    else put_rem_intra_luma_pred_mode(out, p.rem_intra_luma_pred_mode);
    put_intra_chroma_pred_mode(out, p.intra_chroma_pred_mode);
    if (p.emit_split_transform_flag)
        put_split_transform_flag(out, p.log2_trafo_size, p.split_transform_flag ? 1 : 0);
    if (p.emit_chroma_cbf) {
        put_cbf_cb_cr(out, 1, p.trafo_depth, p.cbf_cb);
        put_cbf_cb_cr(out, 2, p.trafo_depth, p.cbf_cr);
    }
    put_cbf_luma(out, p.trafo_depth, p.cbf_luma);
    return out;
}

bool decode_cu_intra_bins(BinCursor& c, const CuIntraBins& shape, CuIntraBins* out) {
    if (!out) return false;
    *out = shape;
    int v = 0;
    if (shape.emit_part_mode) {
        bool pm = false;
        if (!get_part_mode_intra(c, pm)) return false;
        out->part_2nx2n = pm;
    }
    if (!get_prev_intra_luma_pred_flag(c, v)) return false;
    out->prev_intra_luma_pred_flag = (v != 0);
    if (out->prev_intra_luma_pred_flag) {
        if (!get_mpm_idx(c, v)) return false;
        out->mpm_idx = v;
    } else {
        if (!get_rem_intra_luma_pred_mode(c, v)) return false;
        out->rem_intra_luma_pred_mode = v;
    }
    if (!get_intra_chroma_pred_mode(c, v)) return false;
    out->intra_chroma_pred_mode = v;
    if (shape.emit_split_transform_flag) {
        if (!get_split_transform_flag(c, shape.log2_trafo_size, v)) return false;
        out->split_transform_flag = (v != 0);
    }
    if (shape.emit_chroma_cbf) {
        if (!get_cbf_cb_cr(c, 1, shape.trafo_depth, v)) return false;
        out->cbf_cb = v;
        if (!get_cbf_cb_cr(c, 2, shape.trafo_depth, v)) return false;
        out->cbf_cr = v;
    }
    if (!get_cbf_luma(c, shape.trafo_depth, v)) return false;
    out->cbf_luma = v;
    return true;
}

// ── B 片帧间语法元素 ────────────────────────────────────────────────────
void put_ref_idx_l1(std::vector<Bin>& out, int idx, int nb_refs) {
    // ffmpeg ff_hevc_ref_idx_lx_decode：单参考时不读，多参考用一元递增 + bypass。
    if (nb_refs <= 1) return;
    int v = idx < 0 ? 0 : idx;
    if (v > nb_refs - 1) v = nb_refs - 1;
    for (int i = 0; i < v; ++i) out.push_back({kCtxRefIdxL1 + (i < 2 ? i : 1), 1});
    if (v < nb_refs - 1) out.push_back({kCtxRefIdxL1 + (v < 2 ? v : 1), 0});
}

void put_inter_pred_idc(std::vector<Bin>& out, int ct_depth, int idc, bool small_pb) {
    const int base = kCtxInterPredIdc;
    if (small_pb) {
        out.push_back({base + 4, idc == 2 ? 1 : 0});
        return;
    }
    const int cd = ct_depth < 0 ? 0 : (ct_depth > 3 ? 3 : ct_depth);
    if (idc == 2) {
        out.push_back({base + cd, 1});
        return;
    }
    out.push_back({base + cd, 0});
    out.push_back({base + 4, idc == 1 ? 1 : 0});
}

bool get_inter_pred_idc(BinCursor& c, int ct_depth, bool small_pb, int& idc) {
    const int base = kCtxInterPredIdc;
    if (small_pb) {
        int v = 0;
        if (!c.decision(base + 4, v)) return false;
        idc = v ? 2 : 0;
        return true;
    }
    const int cd = ct_depth < 0 ? 0 : (ct_depth > 3 ? 3 : ct_depth);
    int v = 0;
    if (!c.decision(base + cd, v)) return false;
    if (v) {
        idc = 2;
        return true;
    }
    if (!c.decision(base + 4, v)) return false;
    idc = v ? 1 : 0;
    return true;
}

}  // namespace termchart::image::codec::hevc
