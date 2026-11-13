// parameter_sets.cpp —— VPS / SPS / PPS 位序列（A2：从 hevc_encode.cpp 原样迁入）
#include "parameter_sets.hpp"

#include "../bitstream/bit_writer.hpp"

namespace termchart::image::codec::hevc {
namespace {

using bitstream::BitWriter;

// profile_tier_level（MaxNumSubLayersMinus1 = 0，无子层循环）。
// HEVC 7.3.3：Main(1) 与 Range Extensions(4) 都走同一段 else 分支——4 个约束标志
// 之后是 44 个 0（Main 叫 general_reserved_zero_44bits，RExt 是 general_*_constraint
// 全 0），位宽一致。二者只差 profile_idc 与兼容标志。
// RExt(4) 是 transform_skip 超出 4x4 的必要条件：FFmpeg 只在 profile_idc >= 4 时
// 解析 PPS range extension（ps.c:2431），否则 log2_max_transform_skip_block_size
// 恒按默认 2 处理，8/16/32 TU 的 transform_skip_flag 不会被读。
void profile_tier_level(BitWriter& w, int level_idc, int profile_idc) {
    // 兼容标志 bit1 = Main、bit4 = RExt。RExt 档同时声明 Main 兼容：4:2:0 8bit 的
    // RExt 码流本身满足 Main 的全部约束，硬解/播放器更不容易直接拒流。
    const unsigned compat = (profile_idc >= 4) ? 0x48000000u : 0x40000000u;
    w.u(0, 2);   // general_profile_space
    w.u(0, 1);   // general_tier_flag
    w.u(static_cast<unsigned>(profile_idc), 5);  // general_profile_idc
    w.u(compat, 32);  // general_profile_compatibility_flags
    w.u(1, 1);   // general_progressive_source_flag
    w.u(0, 1);   // general_interlaced_source_flag
    w.u(1, 1);   // general_non_packed_constraint_flag
    w.u(1, 1);   // general_frame_only_constraint_flag
    w.u(0, 44);  // general_reserved_zero_44bits / general_*_constraint_flag（全 0）
    w.u(static_cast<unsigned>(level_idc), 8);  // general_level_idc
}

}  // namespace

std::string write_vps(const Vps& vps) {
    BitWriter w;
    w.u(0, 4);    // vps_video_parameter_set_id
    w.u(1, 1);    // vps_base_layer_internal_flag
    w.u(1, 1);    // vps_base_layer_available_flag
    w.u(0, 6);    // vps_max_layers_minus1
    w.u(0, 3);    // vps_max_sub_layers_minus1
    w.u(1, 1);    // vps_temporal_id_nesting_flag
    w.u(0xFFFF, 16);  // vps_reserved_0xffff_16bits
    profile_tier_level(w, vps.general_level_idc, vps.general_profile_idc);
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
    profile_tier_level(w, sps.general_level_idc, sps.general_profile_idc);
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
    w.ue(0);      // log2_min_luma_transform_block_size_minus2  (→ 4px)
    // MaxTrafoSize 按档位分离，绝不写死：
    // - I_PCM 档用默认 3（MaxTb=32）：G0 黄金基线 20433B/sha 逐字节锁死，改这里即破。
    // - 真压缩档显式设 0（MaxTb=4px）：亮度 TU 恒为 4x4，8x8 CU 必须分裂成 4 个
    //   4x4 TU。此时解码端在 8x8 根节点**不读** split_transform_flag（隐式 split=1），
    //   编码端也不能写该 bin（见 cu_coding.cpp::encode_cu_intra_8x8）；若仍按 32
    //   写，ffmpeg 会把后续 bin 当作 8x8 单 TU 语法解析，整帧解码错位。
    w.ue(static_cast<unsigned>(sps.log2_diff_max_min_transform_block_size));

    w.ue(0);      // max_transform_hierarchy_depth_inter
    w.ue(static_cast<unsigned>(sps.max_transform_hierarchy_depth_intra));  // max_transform_hierarchy_depth_intra
    w.u(0, 1);    // scaling_list_enabled_flag
    w.u(0, 1);    // amp_enabled_flag
    w.u(0, 1);    // sample_adaptive_offset_enabled_flag
    if (sps.pcm_enabled) {
        w.u(1, 1);    // pcm_enabled_flag
        w.u(7, 4);    // pcm_sample_bit_depth_luma_minus1
        w.u(7, 4);    // pcm_sample_bit_depth_chroma_minus1
        w.ue(0);      // log2_min_pcm_luma_coding_block_size_minus3（PCM 块 = 8）
        w.ue(0);      // log2_diff_max_min_pcm_luma_coding_block_size
        w.u(1, 1);    // pcm_loop_filter_disabled_flag
    } else {
        w.u(0, 1);    // pcm_enabled_flag = 0（关 PCM，走 CABAC 编码树）
    }
    w.ue(0);      // num_short_term_ref_pic_sets
    w.u(0, 1);    // long_term_ref_pics_present_flag
    w.u(0, 1);    // sps_temporal_mvp_enabled_flag
    w.u(0, 1);    // strong_intra_smoothing_enabled_flag
    // VUI 最小集（HEVC 7.3.4）：显式声明 1:1 SAR，避免播放器用
    // coded_height(2176) 与 display_height(2160) 推算出 135:136 这类错误 PAR。
    w.u(1, 1);    // vui_parameters_present_flag
    w.u(1, 1);    // aspect_ratio_info_present_flag
    w.u(1, 8);    // aspect_ratio_idc = 1 (SAR 1:1)
    w.u(0, 1);    // overscan_info_present_flag
    w.u(0, 1);    // video_signal_type_present_flag
    w.u(0, 1);    // chroma_loc_info_present_flag
    w.u(0, 1);    // neutral_chroma_indication_flag
    w.u(0, 1);    // field_seq_flag
    w.u(0, 1);    // frame_field_info_present_flag
    w.u(0, 1);    // default_display_window_flag
    w.u(0, 1);    // vui_timing_info_present_flag
    w.u(0, 1);    // bitstream_restriction_flag
    if (sps.range_extension) {
        w.u(1, 1);    // sps_extension_present_flag
        w.u(1, 1);    // sps_range_extension_flag
        w.u(0, 1);    // sps_multilayer_extension_flag
        w.u(0, 1);    // sps_3d_extension_flag
        w.u(0, 1);    // sps_scc_extension_flag
        w.u(0, 4);    // sps_extension_4bits
        // sps_range_extension()：9 个开关全 0。transform_skip_context_enabled_flag=0
        // 是刻意的：置 1 会让 transform_skip 的 sig_coeff_flag 换一组 ctx（ctxSet=2），
        // 残差 bin 序列随之改变；我们用与常规块同一套 ctx，两端一致即可。
        // extended_precision_processing=0 保持 bitDepth 相关舍入不额外偏移。
        w.u(0, 1);    // transform_skip_rotation_enabled_flag
        w.u(0, 1);    // transform_skip_context_enabled_flag
        w.u(0, 1);    // implicit_rdpcm_enabled_flag
        w.u(0, 1);    // explicit_rdpcm_enabled_flag
        w.u(0, 1);    // extended_precision_processing_flag
        w.u(0, 1);    // intra_smoothing_disabled_flag
        w.u(0, 1);    // high_precision_offsets_enabled_flag
        w.u(0, 1);    // persistent_rice_adaptation_enabled_flag
        w.u(0, 1);    // cabac_bypass_alignment_enabled_flag
    } else {
        w.u(0, 1);    // sps_extension_present_flag = 0
    }
    return w.finish();
}

std::string write_pps(const Pps& pps) {
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
    // transform_skip_enabled_flag：默认 0（G0 基线 / 现有 Intra 档字节不变）；
    // 置 1 时解码端会在每个 cbf=1 的 transform_unit 的残差语法最前面多读一个
    // transform_skip_flag（cabac.c:1138-1140）。本 PPS 不写 extension，
    // 故 log2_max_transform_skip_block_size 走 ffmpeg 默认 2（ps.c:2243）。
    w.u(pps.transform_skip_enabled ? 1 : 0, 1);
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
    if (pps.disable_deblocking) {
        w.u(1, 1);    // deblocking_filter_control_present_flag
        w.u(0, 1);    // deblocking_filter_override_enabled_flag（0 ⇒ slice 不读 override）
        w.u(1, 1);    // pps_deblocking_filter_disabled_flag（1 ⇒ 解码端不做去块滤波）
        // disabled=1 时 pps_beta/tc_offset_div2 不出现在码流里，推断为 0（7.3.2.3）。
    } else {
        w.u(0, 1);    // deblocking_filter_control_present_flag（G0 基线口径）
    }
    w.u(0, 1);    // pps_scaling_list_data_present_flag
    w.u(0, 1);    // lists_modification_present_flag
    w.ue(0);      // log2_parallel_merge_level_minus2
    w.u(0, 1);    // slice_segment_header_extension_present_flag
    // pps_extension：只有「transform_skip 允许超过 4x4」才写。
    // 写成 bool 而非固定 0，是因为 log2_max_transform_skip_block_size 只存在于
    // pps_range_extension()（RExt），默认 2 时省略它才能保住既有 PPS 字节。
    const bool write_rext =
        pps.transform_skip_enabled && pps.log2_max_transform_skip_block_size > 2;
    w.u(write_rext ? 1 : 0, 1);    // pps_extension_present_flag
    if (write_rext) {
        w.u(1, 1);    // pps_range_extension_flag
        w.u(0, 1);    // pps_multilayer_extension_flag
        w.u(0, 1);    // pps_3d_extension_flag
        w.u(0, 1);    // pps_scc_extension_flag
        w.u(0, 4);    // pps_extension_4bits
        // pps_range_extension()（HEVC 7.3.2.3.2 / ffmpeg ps.c:1973-2005）：
        // log2_max_transform_skip_block_size_minus2 只在 transform_skip_enabled_flag=1
        // 时出现，且值域 0..3（即最大 32x32）。后续 SAO offset scale 必须为 0
        // （8bit 输入下 ffmpeg 会校验 > max(bitdepth-10,0) 即报错）。
        w.ue(static_cast<unsigned>(pps.log2_max_transform_skip_block_size - 2));
        w.u(0, 1);    // cross_component_prediction_enabled_flag
        w.u(0, 1);    // chroma_qp_offset_list_enabled_flag
        w.ue(0);      // log2_sao_offset_scale_luma
        w.ue(0);      // log2_sao_offset_scale_chroma
    }
    return w.finish();
}

}  // namespace termchart::image::codec::hevc
