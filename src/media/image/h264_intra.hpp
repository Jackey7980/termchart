// h264_intra.hpp —— 帧内宏块压缩档的对外声明（I_4x4 + 整数变换 + CAVLC）
//
// 与 I_PCM 档（h264_encode.cpp 里直接吐原始采样）并列：这里负责把一帧 YUV420
// 编成「帧内预测 + 4x4 整数变换/量化 + CAVLC」的 IDR slice RBSP。NAL 封装、SPS/PPS、
// MP4 封装仍由调用方负责，本文件不碰码流骨架。
#ifndef TERMCHART_SRC_IMAGE_H264_INTRA_HPP
#define TERMCHART_SRC_IMAGE_H264_INTRA_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace termchart::image::h264_detail {

// 一帧 YUV420（BT.601，8bit），尺寸已按 16 对齐补边。
struct YuvFrame {
    int cw = 0;  // 亮度宽（16 的倍数）
    int ch = 0;  // 亮度高（16 的倍数）
    std::vector<uint8_t> y, u, v;
};

// 编一帧 IDR slice 的 RBSP（不含 NAL 头与起始码，也未加防竞争字节）。
// qp ∈ [0,51]；idr_id 写进 idr_pic_id。
std::string build_idr_slice_intra(const YuvFrame& f, int idr_id, int qp);

}  // namespace termchart::image::h264_detail

#endif  // TERMCHART_SRC_IMAGE_H264_INTRA_HPP
