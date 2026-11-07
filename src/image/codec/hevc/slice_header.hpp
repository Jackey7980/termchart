// slice_header.hpp —— HEVC slice_segment_header() 的 POD 结构与写入接口
//
// 头文件只暴露接口与 POD 结构体；字段是否出现在码流里由 PPS/SPS 开关决定，
// 具体位序列在 slice_header.cpp。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_SLICE_HEADER_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_SLICE_HEADER_HPP

#include <string>

namespace termchart::image::codec::hevc {

struct SliceHeader {
    int nal_unit_type = 19;  // IDR_W_RADL
    bool first_slice_segment_in_pic_flag = true;
    bool no_output_of_prior_pics_flag = false;
    int slice_pic_parameter_set_id = 0;
    int slice_type = 2;      // I
    int slice_qp_delta = 0;  // init_qp_minus26=0

    // 由 PPS/SPS 决定的开关。当前全为关闭值，故对应字段不写入码流；
    // 阶段二按 7.3.6.1 的顺序在 write_slice_header() 中补字段。
    int num_extra_slice_header_bits = 0;
    bool output_flag_present_flag = false;
    bool separate_colour_plane_flag = false;
    bool tiles_enabled_flag = false;
    bool entropy_coding_sync_enabled_flag = false;
    bool pps_loop_filter_across_slices_enabled_flag = false;
    bool sample_adaptive_offset_enabled_flag = false;
    bool cabac_init_present_flag = false;
    bool deblocking_filter_control_present_flag = false;
    bool pps_slice_chroma_qp_offsets_present_flag = false;
};

std::string write_slice_header(const SliceHeader& header);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_SLICE_HEADER_HPP
