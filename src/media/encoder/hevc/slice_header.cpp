// slice_header.cpp —— HEVC 7.3.6.1 slice_segment_header() 位序列
//
// 字段顺序严格对齐 FFmpeg libavcodec/hevc/hevcdec.c::hls_slice_header()：
//   first_slice_segment_in_pic_flag → (IRAP) no_output_of_prior_pics_flag →
//   slice_pic_parameter_set_id → slice_type → poc_lsb → 短期 RPS →
//   (temporal MVP) → SAO → [P/B: 参考帧数 / cabac_init / merge cand] →
//   slice_qp_delta → 去块 → 环内滤波 → byte_alignment()。
#include "slice_header.hpp"

#include <cstdint>

#include "../bitstream/bit_writer.hpp"

namespace termchart::image::codec::hevc {
namespace {

using bitstream::BitWriter;

// short_term_ref_pic_set()（HEVC 7.3.8，内联形式：st_rps_bits 已读、
// short_term_ref_pic_set_sps_flag=0 ⇒ 不读 inter_ref_pic_set_prediction_flag）。
void write_short_term_rps(BitWriter& w, const StRefPicSet& rps) {
    w.ue(static_cast<std::uint32_t>(rps.num_negative_pics));
    w.ue(static_cast<std::uint32_t>(rps.num_positive_pics));
    // NumDeltaPocs = num_negative_pics + num_positive_pics ≤ 8 ⇒ 最少 3 bit 的
    // ue 前缀，FFmpeg ps.c 对 delta_poc_s0_minus1 额外要求 |deltaPoc| < 32768。
    for (int i = 0; i < rps.num_negative_pics; ++i) {
        w.ue(static_cast<std::uint32_t>(rps.delta_poc_s0_minus1[i]));
        w.u(rps.used_by_curr_pic_s0_flag[i] ? 1u : 0u, 1);
    }
    for (int i = 0; i < rps.num_positive_pics; ++i) {
        w.ue(static_cast<std::uint32_t>(rps.delta_poc_s1_minus1[i]));
        w.u(rps.used_by_curr_pic_s1_flag[i] ? 1u : 0u, 1);
    }
}

}  // namespace

std::string write_slice_header(const SliceHeader& h) {
    BitWriter w;
    const bool is_irap = h.nal_unit_type >= 16 && h.nal_unit_type <= 23;
    const bool is_i = h.slice_type == 2;

    w.u(h.first_slice_segment_in_pic_flag ? 1u : 0u, 1);
    // IDR/BLA/CRA 首片必须写 no_output_of_prior_pics_flag。
    if (is_irap) w.u(h.no_output_of_prior_pics_flag ? 1u : 0u, 1);
    w.ue(static_cast<std::uint32_t>(h.slice_pic_parameter_set_id));
    for (int i = 0; i < h.num_extra_slice_header_bits; ++i) w.u(0, 1);
    if (h.output_flag_present_flag) w.u(1, 1);
    if (h.separate_colour_plane_flag) w.u(0, 2);
    w.ue(static_cast<std::uint32_t>(h.slice_type));

    if (is_irap) {
        // IDR：HEVC 7.3.6.1 不读 poc_lsb / RPS / temporal MVP，四者全部跳过。
        // 早期版本在此多写 4 bit 会把后续字段整体后移，导致 FFmpeg 丢弃整个 NALU。
    } else {
        w.u(h.slice_pic_order_cnt_lsb,
            static_cast<unsigned>(h.log2_max_pic_order_cnt_lsb));
        // short_term_ref_pic_set_sps_flag = 0 ⇒ 随后内联写 RPS
        // （SPS 的 num_short_term_ref_pic_sets = 0，故没有 SPS 索引可选）。
        w.u(0, 1);
        write_short_term_rps(w, h.short_term_ref_pic_set);
        if (h.sps_temporal_mvp_enabled_flag)
            w.u(h.slice_temporal_mvp_enabled_flag ? 1u : 0u, 1);
    }

    // SAO 开关在 RPS / temporal MVP 之后、P/B 参考帧字段之前（7.3.6.1）。
    if (h.sample_adaptive_offset_enabled_flag) {
        w.u(h.slice_sao_luma_flag ? 1u : 0u, 1);
        // ChromaArrayType = 1（4:2:0）⇒ 必写色度开关。
        w.u(h.slice_sao_chroma_flag ? 1u : 0u, 1);
    }

    if (!is_i) {
        const bool is_b = h.slice_type == 0;
        // FFmpeg 对 P/B 无条件读 num_ref_idx_active_override_flag。
        w.u(h.num_ref_idx_active_override_flag ? 1u : 0u, 1);
        if (h.num_ref_idx_active_override_flag) {
            w.ue(static_cast<std::uint32_t>(h.num_ref_idx_l0_active_minus1));
            // B 片：override=1 时还要写 l1 计数（7.3.6.1）。
            if (is_b) w.ue(static_cast<std::uint32_t>(h.num_ref_idx_l1_active_minus1));
        }
        // B 片才有 mvd_l1_zero_flag，位置在 override 之后、cabac_init 之前。
        if (is_b) w.u(h.mvd_l1_zero_flag ? 1u : 0u, 1);
        if (h.cabac_init_present_flag) w.u(0, 1);  // cabac_init_flag = 0
        // slice_temporal_mvp_enabled_flag=0 ⇒ 无 collocated_ref_idx。
        // weighted_pred_flag=0（PPS）⇒ 无 pred_weight_table()。
        w.ue(static_cast<std::uint32_t>(h.five_minus_max_num_merge_cand));
    }

    w.se(h.slice_qp_delta);
    if (h.pps_slice_chroma_qp_offsets_present_flag) {
        w.se(0);  // slice_cb_qp_offset
        w.se(0);  // slice_cr_qp_offset
    }
    // deblocking_filter_override_enabled_flag 恒 0 ⇒ 不写 override / disabled /
    // beta / tc，解码端继承 PPS 的 disable_dbf。
    // pps_loop_filter_across_slices_enabled_flag=0 ⇒ 该字段被推断，不写。
    // byte_alignment()：先 1 再补 0 到字节边界（finish() 内完成）。
    return w.finish();
}

}  // namespace termchart::image::codec::hevc
