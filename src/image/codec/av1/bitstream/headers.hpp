// headers.hpp —— AV1 最小 sequence_header / frame_header（8-bit 4:2:0, still picture）
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_HEADERS_HPP
#define TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_HEADERS_HPP

#include <string>

namespace termchart::image::codec::av1 {

struct SequenceHeaderConfig {
    int width = 8;
    int height = 8;
};

struct FrameHeaderConfig {
    int base_q_idx = 100;  // >0 ⇒ 非无损，tx_mode 可由 1 bit 选 LARGEST
};

// 返回 OBU payload（未加 OBU 头）；reduced_still_picture_header=1 分支。
std::string write_sequence_header(const SequenceHeaderConfig& cfg);
std::string write_frame_header(const FrameHeaderConfig& cfg);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_HEADERS_HPP
