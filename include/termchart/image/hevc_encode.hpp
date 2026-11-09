// hevc_encode.hpp —— 自写 HEVC（H.265, Main profile）编码器
//
// 零依赖：自己写 NAL/VPS/SPS/PPS/slice 语法、YUV 转换与码流封装，不调用 libx265/ffmpeg。
// 三个档位：
//   Pcm   —— I_PCM 无损档，字节级稳定，作为 diag_hevc.py 的黄金回归基线；
//   Intra —— 阶段一真帧内压缩：35 种亮度模式逐 CU 决策 + 4x4 变换量化 + 残差 CABAC，
//            三套系数扫描随模式切换，已通过 FFmpeg 软/硬解逐像素门禁；
//   Inter —— 阶段二占位：帧间预测与参考帧管理尚未实现，请求该档**显式返回空串**，
//            不静默降级到 Intra/Pcm（fail-loud 契约）。
#ifndef TERMCHART_IMAGE_HEVC_ENCODE_HPP
#define TERMCHART_IMAGE_HEVC_ENCODE_HPP

#include <span>
#include <string>

#include "termchart/export.hpp"
#include "termchart/image/bitmap.hpp"
#include "termchart/image/encode.hpp"

namespace termchart::image {

// 按 EncodeOptions::video_mode 分派档位。默认 Pcm：输出完整独立的 I_PCM 码流
// （VPS + SPS + PPS + 每帧一个 IDR slice），字节级稳定，供 diag_hevc.py 做黄金回归。
// Intra 已落地；Inter 未落地时返回空串，不静默降级。
TERMCHART_API std::string encode_hevc_annexb(std::span<const Bitmap> frames, const EncodeOptions& opts = {});

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_HEVC_ENCODE_HPP
