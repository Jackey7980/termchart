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

bool large_nonzero(const std::int16_t* c, int size) {
    if (!c) return false;
    for (int i = 0; i < size * size; ++i)
        if (c[i] != 0) return true;
    return false;
}

// 自动档的根节点 split_transform_flag 位于 CU 语法之后、色度 cbf 之前
// （FFmpeg hevcdec.c hls_transform_tree 的读取顺序）。
void put_split_if_needed(std::vector<Bin>& out, const CuIntra8x8& cu) {
    if (cu.emit_split_transform_flag)
        put_split_transform_flag(out, cu.log2_root, cu.split_transform_flag ? 1 : 0);
}

// 单 TU（log2=3/4/5）的 CU 语法：与 4x4 路径共用 CU 头 + 根部色度 cbf，
// 差别只在 cbf_luma 用 trafoDepth=0 的 ctx、亮度残差为多 CG 序列，且色度
// 残差紧跟本 TU 的亮度残差（FFmpeg hls_transform_unit log2>2 分支）。
std::vector<Bin> encode_cu_intra_tuN(const CuIntra8x8& cu) {
    const int lg = cu.log2_tu_size;
    const int n = 1 << lg;
    const int clg = lg - 1;  // 4:2:0 色度 TU
    std::vector<Bin> out;
    if (cu.emit_part_mode) put_part_mode_intra(out, cu.part_2nx2n);
    put_prev_intra_luma_pred_flag(out, cu.prev_intra_luma_pred_flag ? 1 : 0);
    if (cu.prev_intra_luma_pred_flag) put_mpm_idx(out, cu.mpm_idx);
    else put_rem_intra_luma_pred_mode(out, cu.rem_intra_luma_pred_mode);
    put_intra_chroma_pred_mode(out, cu.intra_chroma_pred_mode);
    put_split_if_needed(out, cu);
    put_cbf_cb_cr(out, 1, 0, cu.cbf_cb);
    put_cbf_cb_cr(out, 2, 0, cu.cbf_cr);

    const bool nz = large_nonzero(cu.luma_large, n);
    put_cbf_luma(out, 0, nz ? 1 : 0);
    if (nz) {
        // transform_skip_flag 是 residual_coding() 的第一个语法元素（cabac.c:1138），
        // 朗读条件为 pps.transform_skip_enabled_flag && log2_trafo_size <=
        // log2_max_transform_skip_block_size。大 TU 能不能出现这一 bin 由参数集
        // 声明（PPS range extension），此处只按 cu.transform_skip_enabled 行事。
        // flag=1 不改残差 bin 序列：range extension 里 transform_skip_context 与
        // rotation 都关，ctx_idx_map / Rice / 扫描表与常规块同一套。
        if (cu.transform_skip_enabled)
            put_transform_skip_flag(out, 0, cu.transform_skip_luma[0]);
        bool ok = true;
        const auto rb = encode_residual(cu.luma_large, lg, &ok, cu.scan_idx, 0);
        if (!ok || rb.empty()) return {};
        out.insert(out.end(), rb.begin(), rb.end());
    }
    for (int c = 0; c < 2; ++c) {
        const int cbf = c ? cu.cbf_cr : cu.cbf_cb;
        if (!cbf) continue;
        bool ok = true;
        std::vector<Bin> rb;
        if (lg == 3) {
            if (!block_nonzero(cu.chroma4x4[c])) return {};
            if (cu.transform_skip_enabled)
                put_transform_skip_flag(out, c + 1, cu.transform_skip_chroma[c]);
            rb = encode_residual_4x4(cu.chroma4x4[c], &ok, cu.scan_idx, c + 1);
        } else {
            if (!large_nonzero(cu.chroma_large[c], 1 << clg)) return {};
            // 色度 TU 的朗读条件是 log2_trafo_size_c(=clg) <=
            // log2_max_transform_skip_block_size。调用方保证最大档 >= 亮度 log2，
            // 故 clg 恒被覆盖；此处仍按同一个 cu.transform_skip_enabled 写 flag。
            if (cu.transform_skip_enabled)
                put_transform_skip_flag(out, c + 1, cu.transform_skip_chroma[c]);
            rb = encode_residual(cu.chroma_large[c], clg, &ok, cu.scan_idx, c + 1);
        }
        if (!ok || rb.empty()) return {};
        out.insert(out.end(), rb.begin(), rb.end());
    }
    return out;
}

}  // namespace

std::vector<Bin> encode_cu_intra_8x8(const CuIntra8x8& cu) {
    // PART_NxN 需要 4 个独立的 intra prediction unit（各 4x4 PU 各写一套
    // prev_intra_luma_pred_flag / mpm_idx|rem），当前只实现 PART_2Nx2N。
    // 写出去会被解码端按 4 套 PU 语法读，序列必然错位，故显式拒绝。
    if (!cu.part_2nx2n) return {};
    if (cu.log2_tu_size >= 3 && cu.log2_tu_size <= 5) return encode_cu_intra_tuN(cu);
    if (cu.log2_tu_size != 2) return {};

    std::vector<Bin> out;
    put_part_mode_intra(out, cu.part_2nx2n);
    put_prev_intra_luma_pred_flag(out, cu.prev_intra_luma_pred_flag ? 1 : 0);
    if (cu.prev_intra_luma_pred_flag) put_mpm_idx(out, cu.mpm_idx);
    else put_rem_intra_luma_pred_mode(out, cu.rem_intra_luma_pred_mode);
    put_intra_chroma_pred_mode(out, cu.intra_chroma_pred_mode);
    put_split_if_needed(out, cu);

    // 根 transform_tree（log2_trafo_size=3、trafoDepth=0）：
    // SPS 中 log2_max_trafo_size == 2（MaxTb=4x4），解码端的读条件
    // `log2_trafo_size <= log2_max_trafo_size` 即 3 <= 2 不成立，split_transform_flag
    // **一个 bin 都不读**、隐式按 1 处理。此处若仍写这一 bin，解码端会把它当作
    // 下一个语法元素，从本 CU 起整条 slice 的算术状态与 bin 语义全部错位。
    //
    // 色度 cbf 在根节点、且位于分裂递归**之前**读出：
    // log2_trafo_size=3 > 2 故此处读 cbf_cb/cbf_cr；子层 4x4 不再读色度 cbf。
    put_cbf_cb_cr(out, 1, 0, cu.cbf_cb);  // 根节点 trafo_depth=0 → ctx 42
    put_cbf_cb_cr(out, 2, 0, cu.cbf_cr);

    // 四个 4x4 叶子 TU（trafoDepth=1）：cbf_luma 用 ctx 40。
    for (int i = 0; i < 4; ++i) {
        const bool nz = block_nonzero(cu.luma4x4[i]);
        put_cbf_luma(out, 1, nz ? 1 : 0);
        if (!nz) continue;
        // ffmpeg hls_residual_coding 在 cbf=1 时的第一个语法元素（cabac.c:1138）。
        // transform_skip_flag=1 只改逆量化/重建，不改残差 bin 序列本身：本 SPS
        // 不开 range extension ⇒ transform_skip_context/rotation=0，PPS
        // sign_data_hiding=0，扫描表 / ctx_idx_map / Rice 全部沿用 4x4 路径。
        if (cu.transform_skip_enabled)
            put_transform_skip_flag(out, 0, cu.transform_skip_luma[i]);
        bool ok = true;
        const auto rb = encode_residual_4x4(cu.luma4x4[i], &ok, cu.scan_idx);
        if (!ok || rb.empty()) return {};
        out.insert(out.end(), rb.begin(), rb.end());
    }

    // FFmpeg hevcdec.c:1484-1506：亮度 TU 为 4x4（MaxTb=4）时，色度残差在
    // blk_idx==3 的那个 transform_unit 内、亮度残差之后依次写 U(c_idx=1) / V(c_idx=2)。
    // 色度 intra_pred 不产生 bin，故这里只追加两个 residual_coding 的 bin 序列。
    for (int c = 0; c < 2; ++c) {
        const int cbf = c ? cu.cbf_cr : cu.cbf_cb;
        if (!cbf) continue;
        if (!block_nonzero(cu.chroma4x4[c])) return {};  // cbf=1 却全零是非法码流
        if (cu.transform_skip_enabled)
            put_transform_skip_flag(out, c + 1, cu.transform_skip_chroma[c]);
        bool ok = true;
        const auto rb = encode_residual_4x4(cu.chroma4x4[c], &ok, cu.scan_idx, c + 1);
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

    // 与 encode_cu_intra_8x8 同序：根节点先读色度 cbf（在分裂递归之前），
    // 再进入四个 4x4 叶子读 cbf_luma。MaxTb=4x4 时解码端不读 split_transform_flag，
    // 故 shape.emit_split_transform_flag 仅保留"隐式分裂"语义，不再产生 bin。
    if (shape.emit_chroma_cbf) {
        if (!get_cbf_cb_cr(c, 1, 0, v)) return false;
        out->cbf_cb = v;
        if (!get_cbf_cb_cr(c, 2, 0, v)) return false;
        out->cbf_cr = v;
    }

    for (int i = 0; i < 4; ++i) {
        if (!get_cbf_luma(c, 1, v)) return false;
        out->cbf_luma[i] = v;
        if (!v) continue;
        if (shape.transform_skip_enabled &&
            !get_transform_skip_flag(c, 0, out->transform_skip_luma[i])) return false;
        std::size_t used = 0;
        if (!decode_residual_4x4(c.current(), c.remaining(), out->luma[i], &used,
                                 shape.scan_idx)) return false;
        c.advance(used);
    }

    for (int ch = 0; ch < 2; ++ch) {
        const int cbf = ch ? out->cbf_cr : out->cbf_cb;
        if (!cbf) continue;
        if (shape.transform_skip_enabled &&
            !get_transform_skip_flag(c, ch + 1, out->transform_skip_chroma[ch])) return false;
        std::size_t used = 0;
        if (!decode_residual_4x4(c.current(), c.remaining(), out->chroma[ch], &used,
                                 shape.scan_idx, ch + 1)) return false;
        c.advance(used);
    }
    if (consumed) *consumed = c.consumed();
    return true;
}

}  // namespace termchart::image::codec::hevc
