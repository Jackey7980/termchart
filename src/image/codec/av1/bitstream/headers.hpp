// headers.hpp —— AV1 最小 sequence_header / frame_header（8-bit 4:2:0, still picture）
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_HEADERS_HPP
#define TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_HEADERS_HPP

#include <string>

namespace termchart::image::codec::av1 {

struct SequenceHeaderConfig {
    int width = 8;
    int height = 8;
    // false 保持 reduced_still_picture_header 的极简帧内序列头；true 写出
    // 可承载 INTER_FRAME 的完整序列头（无 timing/order_hint，screen tools 自适应）。
    bool inter_frames = false;
};

struct FrameHeaderConfig {
    int base_q_idx = 100;  // >0 ⇒ 非无损，tx_mode 可由 1 bit 选 LARGEST
    int width = 8;         // 仅用于推导 tile_info 的 max_log2_cols / max_log2_rows
    int height = 8;
    int frame_type = 0;       // 0 = KEY_FRAME，1 = INTER_FRAME
    int global_mv_x = 0;      // INTER_FRAME 的全局平移 MV（1/8 像素）
    int global_mv_y = 0;
};

// 返回 OBU payload（未加 OBU 头）；reduced_still_picture_header=1 分支。
std::string write_sequence_header(const SequenceHeaderConfig& cfg);
std::string write_frame_header(const FrameHeaderConfig& cfg);

// 阶段五：非 reduced_still_picture_header 的序列头与 INTER_FRAME 帧头。
std::string write_sequence_header_inter(const SequenceHeaderConfig& cfg);
std::string write_frame_header_inter(const FrameHeaderConfig& cfg);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_HEADERS_HPP
