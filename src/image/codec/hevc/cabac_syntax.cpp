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

}  // namespace termchart::image::codec::hevc
