// cu_coding.cpp —— 8x8 帧内 CU 的 bin 序列组装
#include "cu_coding.hpp"

#include "cabac_context.hpp"
#include "residual_coding.hpp"

namespace termchart::image::codec::hevc {

namespace {

bool block_nonzero(const std::int16_t* c) {
    if (!c) return false;
    for (int i = 0; i < 16; ++i)
        if (c[i] != 0) return true;
    return false;
}

}  // namespace

std::vector<Bin> encode_cu_intra_8x8(const CuIntra8x8& cu) {
    // PART_NxN 需要 4 个独立的 intra prediction unit（各 4x4 PU 各写一套
    // prev_intra_luma_pred_flag / mpm_idx|rem），当前只实现 PART_2Nx2N。
    // 写出去会被解码端按 4 套 PU 语法读，序列必然错位，故显式拒绝。
    if (!cu.part_2nx2n) return {};
    if (cu.cbf_cb != 0 || cu.cbf_cr != 0) return {};  // 色度残差未实现

    std::vector<Bin> out;
    put_part_mode_intra(out, cu.part_2nx2n);
    put_prev_intra_luma_pred_flag(out, cu.prev_intra_luma_pred_flag ? 1 : 0);
    if (cu.prev_intra_luma_pred_flag) put_mpm_idx(out, cu.mpm_idx);
    else put_rem_intra_luma_pred_mode(out, cu.rem_intra_luma_pred_mode);
    put_intra_chroma_pred_mode(out, cu.intra_chroma_pred_mode);

    // 根 transform_tree：8x8 CU 拆成 4 个 4x4 TU。
    put_split_transform_flag(out, 3, 1);
    put_cbf_cb_cr(out, 0, 0);
    put_cbf_cb_cr(out, 0, 0);

    for (int i = 0; i < 4; ++i) {
        const bool nz = block_nonzero(cu.luma4x4[i]);
        put_cbf_luma(out, 1, nz ? 1 : 0);  // trafo_depth=1 -> ctx 40
        if (!nz) continue;
        bool ok = true;
        const auto rb = encode_residual_4x4(cu.luma4x4[i], &ok);
        if (!ok || rb.empty()) return {};
        out.insert(out.end(), rb.begin(), rb.end());
    }
    return out;
}

bool decode_cu_intra_8x8(const Bin* bins, std::size_t count, const CuIntra8x8Shape& shape,
                         CuIntra8x8Decoded* out, std::size_t* consumed) {
    if (!out) return false;
    *out = CuIntra8x8Decoded();
    BinCursor c(bins, count);

    bool pm = false;
    if (!get_part_mode_intra(c, pm)) return false;
    out->part_2nx2n = pm;

    int v = 0;
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
        if (!get_split_transform_flag(c, 3, v)) return false;
        if (v != 1) return false;  // 8x8 只支持拆成 4x4
    }
    if (shape.emit_chroma_cbf) {
        if (!get_cbf_cb_cr(c, 0, v)) return false;
        out->cbf_cb = v;
        if (!get_cbf_cb_cr(c, 0, v)) return false;
        out->cbf_cr = v;
    }

    for (int i = 0; i < 4; ++i) {
        if (!get_cbf_luma(c, 1, v)) return false;
        out->cbf_luma[i] = v;
        if (!v) continue;
        std::size_t used = 0;
        if (!decode_residual_4x4(c.current(), c.remaining(), out->luma[i], &used)) return false;
        c.advance(used);
    }
    if (consumed) *consumed = c.consumed();
    return true;
}

}  // namespace termchart::image::codec::hevc
