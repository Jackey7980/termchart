// h264_encode.hpp —— 自写 H.264（baseline AVC）编码器
//
// 零依赖：自己写 NAL/SPS/PPS/slice 语法、YUV 转换与码流封装，不调用 libx264/ffmpeg。
// 当前实现为 **I_PCM 档**：每个宏块直接输出原始采样（无损、无变换/量化/熵编码），
// 用最少的码流语法把整条 YUV→NAL→MP4 链路打通并能被 ffprobe 解码。帧内预测 +
// 整数变换 + CAVLC 压缩档作为「高质量」第二阶段叠加，复用同一 NAL/容器骨架。
#ifndef TERMCHART_IMAGE_H264_ENCODE_HPP
#define TERMCHART_IMAGE_H264_ENCODE_HPP

#include <span>
#include <string>

#include "termchart/export.hpp"
#include "termchart/image/bitmap.hpp"
#include "termchart/image/encode.hpp"

namespace termchart::image {

// 输出 Annex-B 字节流：SPS + PPS + 每帧一个 IDR slice（I_PCM 宏块）。空输入返回空串。
TERMCHART_API std::string encode_h264_annexb(std::span<const Bitmap> frames, const EncodeOptions& opts = {});

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_H264_ENCODE_HPP
