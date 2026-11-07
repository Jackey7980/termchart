// parameter_sets.cpp —— VPS / SPS / PPS 位序列（A2：从 hevc_encode.cpp 原样迁入）
#include "parameter_sets.hpp"

#include "../bitstream/bit_writer.hpp"

namespace termchart::image::codec::hevc {
namespace {

using bitstream::BitWriter;

// profile_tier_level（MaxNumSubLayersMinus1 = 0，无子层循环）。
// HEVC 7.3.3：general_profile_idc==1（Main）走 else 分支 → 4 个约束标志之后是
// general_reserved_zero_44bits（44 个 0），绝不可把 Main 专属 max_*_constraint 标志写进该区。
void profile_tier_level(BitWriter& w) {
    w.u(0, 2);   // general_profile_space
    w.u(0, 1);   // general_tier_flag
    w.u(1, 5);   // general_profile_idc = 1 (Main, 8-bit)
    w.u(2, 32);  // general_profile_compatibility_flags：bit1 置位（Main 兼容）
    w.u(1, 1);   // general_progressive_source_flag
    w.u(0, 1);   // general_interlaced_source_flag
    w.u(1, 1);   // general_non_packed_constraint_flag
    w.u(1, 1);   // general_frame_only_constraint_flag
    w.u(0, 44);  // general_reserved_zero_44bits（Main profile = else 分支，全 0）
    w.u(30, 8);  // general_level_idc = 30
}

}  // namespace

std::string write_vps(const Vps&) {
    BitWriter w;
    w.u(0, 4);    // vps_video_parameter_set_id
    w.u(1, 1);    // vps_base_layer_internal_flag
    w.u(1, 1);    // vps_base_layer_available_flag
    w.u(0, 6);    // vps_max_layers_minus1
    w.u(0, 3);    // vps_max_sub_layers_minus1
    w.u(1, 1);    // vps_temporal_id_nesting_flag
    w.u(0xFFFF, 16);  // vps_reserved_0xffff_16bits
    profile_tier_level(w);
    w.u(1, 1);    // vps_sub_layer_ordering_info_present_flag
    w.ue(0);      // vps_max_dec_pic_buffering_minus1[0]
    w.ue(0);      // vps_max_num_reorder_pics[0]
    w.ue(0);      // vps_max_latency_increase_plus1[0]
    w.u(0, 6);    // vps_max_layer_id
    w.ue(0);      // vps_num_layer_sets_minus1
    w.u(0, 1);    // vps_timing_info_present_flag
    w.u(0, 1);    // vps_extension_present_flag
    return w.finish();
}

std::string write_sps(const Sps& sps) {
    BitWriter w;
    w.u(0, 4);    // sps_video_parameter_set_id
    w.u(0, 3);    // sps_max_sub_layers_minus1
    w.u(1, 1);    // sps_temporal_id_nesting_flag
    profile_tier_level(w);
    w.ue(0);      // sps_seq_parameter_set_id
    w.ue(1);      // chroma_format_idc = 1 (4:2:0)
    w.ue(static_cast<unsigned>(sps.pic_width_in_luma_samples));
    w.ue(static_cast<unsigned>(sps.pic_height_in_luma_samples));
    // conformance_window_flag：CTB 对齐产生底部 padding 时写裁剪窗口，
    // 让解码端输出原始 W×H 而不是补齐后的 cw×ch。
    if (sps.conf_win_bottom > 0) {
        w.u(1, 1);
        w.ue(0);  // conf_win_left_offset
        w.ue(0);  // conf_win_right_offset
        w.ue(0);  // conf_win_top_offset
        w.ue(static_cast<unsigned>(sps.conf_win_bottom));  // conf_win_bottom_offset
    } else {
        w.u(0, 1);
    }
    w.ue(0);      // bit_depth_luma_minus8
    w.ue(0);      // bit_depth_chroma_minus8
    w.ue(0);      // log2_max_pic_order_cnt_lsb_minus4
    w.u(0, 1);    // sps_sub_layer_ordering_info_present_flag = 0
    w.ue(0);      // sps_max_dec_pic_buffering_minus1（层 0）
    w.ue(0);      // sps_max_num_reorder_pics（层 0）
    w.ue(0);      // sps_max_latency_increase_plus1（层 0）
    // CTB = 64（MinCb=8, MaxCb=8+diff3=64）；log2_min_cb_minus3=0、diff_max_min=3。
    w.ue(0);      // log2_min_luma_coding_block_size_minus3
    w.ue(3);      // log2_diff_max_min_luma_coding_block_size
    w.ue(0);      // log2_min_luma_transform_block_size_minus2
    w.ue(3);      // log2_diff_max_min_luma_transform_block_size
    w.ue(0);      // max_transform_hierarchy_depth_inter
    w.ue(0);      // max_transform_hierarchy_depth_intra
    w.u(0, 1);    // scaling_list_enabled_flag
    w.u(0, 1);    // amp_enabled_flag
    w.u(0, 1);    // sample_adaptive_offset_enabled_flag
    w.u(1, 1);    // pcm_enabled_flag
    w.u(7, 4);    // pcm_sample_bit_depth_luma_minus1
    w.u(7, 4);    // pcm_sample_bit_depth_chroma_minus1
    w.ue(0);      // log2_min_pcm_luma_coding_block_size_minus3（PCM 块 = 8）
    w.ue(0);      // log2_diff_max_min_pcm_luma_coding_block_size
    w.u(1, 1);    // pcm_loop_filter_disabled_flag
    w.ue(0);      // num_short_term_ref_pic_sets
    w.u(0, 1);    // long_term_ref_pics_present_flag
    w.u(0, 1);    // sps_temporal_mvp_enabled_flag
    w.u(0, 1);    // strong_intra_smoothing_enabled_flag
    w.u(0, 1);    // vui_parameters_present_flag
    w.u(0, 1);    // sps_extension_present_flag
    return w.finish();
}

std::string write_pps(const Pps&) {
    BitWriter w;
    w.ue(0);      // pps_pic_parameter_set_id
    w.ue(0);      // pps_seq_parameter_set_id
    w.u(0, 1);    // dependent_slice_segments_enabled_flag
    w.u(0, 1);    // output_flag_present_flag
    w.u(0, 3);    // num_extra_slice_header_bits（3 位）
    w.u(0, 1);    // sign_data_hiding_enabled_flag
    w.u(0, 1);    // cabac_init_present_flag
    w.ue(0);      // num_ref_idx_l0_default_active_minus1
    w.ue(0);      // num_ref_idx_l1_default_active_minus1
    w.se(0);      // init_qp_minus26
    w.u(0, 1);    // constrained_intra_pred_flag
    w.u(0, 1);    // transform_skip_enabled_flag
    w.u(0, 1);    // cu_qp_delta_enabled_flag
    w.se(0);      // pps_cb_qp_offset
    w.se(0);      // pps_cr_qp_offset
    w.u(0, 1);    // pps_slice_chroma_qp_offsets_present_flag
    w.u(0, 1);    // weighted_pred_flag
    w.u(0, 1);    // weighted_bipred_flag
    w.u(0, 1);    // transquant_bypass_enabled_flag
    w.u(0, 1);    // tiles_enabled_flag
    w.u(0, 1);    // entropy_coding_sync_enabled_flag
    w.u(0, 1);    // pps_loop_filter_across_slices_enabled_flag
    w.u(0, 1);    // deblocking_filter_control_present_flag
    w.u(0, 1);    // pps_scaling_list_data_present_flag
    w.u(0, 1);    // lists_modification_present_flag
    w.ue(0);      // log2_parallel_merge_level_minus2
    w.u(0, 1);    // slice_segment_header_extension_present_flag
    w.u(0, 1);    // pps_extension_present_flag
    return w.finish();
}

}  // namespace termchart::image::codec::hevc
