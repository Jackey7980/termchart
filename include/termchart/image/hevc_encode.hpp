// hevc_encode.hpp —— 自写 HEVC（H.265, Main profile）编码器
//
// 零依赖：自己写 NAL/VPS/SPS/PPS/slice 语法、YUV 转换与码流封装，不调用 libx265/ffmpeg。
// 当前实现与 h264_encode 同哲学的 **I_PCM 档**：每个 Coding Tree Unit 递归二分到 8x8
// 叶，叶直接以 PCM 模式输出原始采样（无损、无变换/量化/熵编码），用最少的码流语法把
// 整条 YUV→NAL→MP4 链路打通并能被 ffprobe 解码。帧间预测 + 真正压缩作为第二阶段叠加，
// 复用同一 NAL/容器骨架。
#ifndef TERMCHART_IMAGE_HEVC_ENCODE_HPP
#define TERMCHART_IMAGE_HEVC_ENCODE_HPP

#include <span>
#include <string>

#include "termchart/export.hpp"
#include "termchart/image/bitmap.hpp"
#include "termchart/image/encode.hpp"

namespace termchart::image {

// 输出 Annex-B 字节流：VPS + SPS + PPS + 每帧一个 IDR slice（I_PCM CTU）。空输入返回空串。
TERMCHART_API std::string encode_hevc_annexb(std::span<const Bitmap> frames, const EncodeOptions& opts = {});

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_HEVC_ENCODE_HPP
