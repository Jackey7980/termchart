// slice_header.hpp —— HEVC slice_segment_header() 的 POD 结构与写入接口
//
// 头文件只暴露接口与 POD 结构体；字段是否出现在码流里由 PPS/SPS 开关决定，
// 具体位序列在 slice_header.cpp。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_SLICE_HEADER_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_SLICE_HEADER_HPP

#include <cstdint>
#include <string>

namespace termchart::image::codec::hevc {

// short_term_ref_pic_set()（HEVC 7.3.8）。当前编码器只写「内联」形式
// （short_term_ref_pic_set_sps_flag = 0），够用且不需要 SPS 侧维护多套 RPS。
struct StRefPicSet {
    int num_negative_pics = 0;
    int num_positive_pics = 0;
    int delta_poc_s0_minus1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    bool used_by_curr_pic_s0_flag[8] = {false, false, false, false, false, false, false, false};
    int delta_poc_s1_minus1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    bool used_by_curr_pic_s1_flag[8] = {false, false, false, false, false, false, false, false};
};

struct SliceHeader {
    int nal_unit_type = 19;  // IDR_W_RADL
    bool first_slice_segment_in_pic_flag = true;
    bool no_output_of_prior_pics_flag = false;
    int slice_pic_parameter_set_id = 0;
    int slice_type = 2;      // 0=B / 1=P / 2=I
    int slice_qp_delta = 0;  // init_qp_minus26=0

    // ---- 帧间（非 IDR）字段 -------------------------------------------------
    // log2_max_pic_order_cnt_lsb（= SPS 的 minus4 + 4）。0 = 不写 poc_lsb
    // （IDR 分支）；非 IDR 必须与 SPS 一致，否则解码端从错误位宽取 POC。
    int log2_max_pic_order_cnt_lsb = 0;
    std::uint32_t slice_pic_order_cnt_lsb = 0;
    // 是否写内联 short_term_ref_pic_set()（非 IDR 必写其一：
    // short_term_ref_pic_set_sps_flag=0 → 内联；本编码器恒走内联）。
    bool short_term_ref_pic_set_inline = false;
    StRefPicSet short_term_ref_pic_set;
    // SPS 声明 temporal MVP 时才写 slice_temporal_mvp_enabled_flag。
    bool sps_temporal_mvp_enabled_flag = false;
    bool slice_temporal_mvp_enabled_flag = false;

    // ---- 由 PPS/SPS 决定的开关 ---------------------------------------------
    int num_extra_slice_header_bits = 0;
    bool output_flag_present_flag = false;
    bool separate_colour_plane_flag = false;
    bool tiles_enabled_flag = false;
    bool entropy_coding_sync_enabled_flag = false;
    bool pps_loop_filter_across_slices_enabled_flag = false;
    bool sample_adaptive_offset_enabled_flag = false;
    // SPS 声明 SAO 时（7.3.6.1）在 RPS 之后写：
    // slice_sao_luma_flag 与 slice_sao_chroma_flag（ChromaArrayType != 0）。
    bool slice_sao_luma_flag = false;
    bool slice_sao_chroma_flag = false;
    bool cabac_init_present_flag = false;
    bool deblocking_filter_control_present_flag = false;
    bool pps_slice_chroma_qp_offsets_present_flag = false;

    // ---- P/B 片附加字段 -----------------------------------------------------
    int num_ref_idx_l0_default_active_minus1 = 0;
    bool num_ref_idx_active_override_flag = false;
    int num_ref_idx_l0_active_minus1 = 0;
    // B 片：override=1 时紧随 l0 计数之后写 num_ref_idx_l1_active_minus1。
    int num_ref_idx_l1_active_minus1 = 0;
    // B 片：mvd_l1_zero_flag（7.3.6.1，位于 override 之后、cabac_init 之前）。
    // true ⇒ inter_pred_idc==PRED_BI 时 MvdL1 推断为 0，码流里不发 mvd_coding。
    bool mvd_l1_zero_flag = false;
    // five_minus_max_num_merge_cand：0 ⇒ 5 个 merge 候选（最大）。
    int five_minus_max_num_merge_cand = 0;
    bool weighted_pred_flag = false;
};

std::string write_slice_header(const SliceHeader& header);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_SLICE_HEADER_HPP
