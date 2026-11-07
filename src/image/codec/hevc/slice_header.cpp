// slice_header.cpp —— HEVC 7.3.6.1 slice_segment_header() 位序列
//
// A2：从 hevc_encode.cpp 原样迁入，按 PPS/SPS 开关条件组织字段。
#include "slice_header.hpp"

#include <cstdint>

#include "../bitstream/bit_writer.hpp"

namespace termchart::image::codec::hevc {

std::string write_slice_header(const SliceHeader& h) {
    bitstream::BitWriter w;
    // HEVC 7.3.6.1 slice_segment_header()（first_slice_segment_in_pic_flag=1，
    // pps.dependent_slice_segments_enabled_flag=0，非 dependent 分支）：
    w.u(h.first_slice_segment_in_pic_flag ? 1u : 0u, 1);
    // IDR/BLA/CRA（nal_unit_type ∈ [BLA_W_LP, RSV_IRAP_VCL23]，本流为 IDR_W_RADL=19）
    // 首片必须读 no_output_of_prior_pics_flag（HEVC 7.3.6.1；FFmpeg 9.0.2 实测会读）。
    if (h.nal_unit_type >= 16 && h.nal_unit_type <= 23)
        w.u(h.no_output_of_prior_pics_flag ? 1u : 0u, 1);
    w.ue(static_cast<std::uint32_t>(h.slice_pic_parameter_set_id));
    // 本 PPS：num_extra_slice_header_bits=0 ⇒ 0 个 slice_reserved_flag；
    // output_flag_present_flag=0 ⇒ 无 pic_output_flag；
    // separate_colour_plane_flag=0 ⇒ 无 colour_plane_id。
    for (int i = 0; i < h.num_extra_slice_header_bits; ++i) w.u(0, 1);
    if (h.output_flag_present_flag) w.u(1, 1);
    if (h.separate_colour_plane_flag) w.u(0, 2);
    w.ue(static_cast<std::uint32_t>(h.slice_type));
    // 关键：nal_unit_type == IDR_W_RADL ⇒ HEVC 7.3.6.1 不读 slice_pic_order_cnt_lsb 与
    // short_term_ref_pic_set（IDR 无参考帧）。早期版本多写这 4 bit 会把后续所有字段整体后移，
    // 最终 byte_alignment 处的 alignment_bit 落到填充 0 上 → "alignment_bit_equal_to_one=0"，
    // FFmpeg 直接跳过整个 slice NALU。故此处【绝不】写 poc_lsb。
    // 同样，本 PPS：tiles_enabled_flag=0 且 entropy_coding_sync_enabled_flag=0 ⇒ 不读
    // num_entry_point_offsets；pps_loop_filter_across_slices_enabled_flag=0 ⇒
    // slice_loop_filter_across_slices_enabled_flag 由解码端推断（不读）；
    // sample_adaptive_offset_enabled_flag=0 ⇒ 无 slice_sao_*_flag；
    // cabac_init_present_flag=0 ⇒ 无 cabac_init_flag；
    // deblocking_filter_control_present_flag=0 ⇒ 无 slice_deblocking_filter_disabled_flag；
    // pps_slice_chroma_qp_offsets_present_flag=0 ⇒ 无 slice_cb/cr_qp_offset。
    // 因此 slice_type 之后本配置下解码端实际只再读一个字段：
    w.se(h.slice_qp_delta);
    // HEVC 7.3.6.1：slice_segment_header() 末尾必须有 byte_alignment()（先写 1 作为
    // alignment_bit_equal_to_one，再补 0 到字节边界），随后 CABAC 从该字节边界开始。
    return w.finish();
}

}  // namespace termchart::image::codec::hevc
