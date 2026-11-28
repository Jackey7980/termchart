// hevc_encode.hpp —— 自写 HEVC（H.265, Main profile）编码器
//
// 零依赖：自己写 NAL/VPS/SPS/PPS/slice 语法、YUV 转换与码流封装，不调用 libx265/ffmpeg。
// 三个档位：
//   Pcm   —— I_PCM 无损档，字节级稳定，作为 diag_hevc.py 的黄金回归基线；
//   Intra —— 阶段一真帧内压缩：35 种亮度模式逐 CU 决策 + 4x4 变换量化 + 残差 CABAC，
//            三套系数扫描随模式切换，已通过 FFmpeg 软/硬解逐像素门禁；
//   Inter —— 阶段二帧间档：P 片真实现，Merge/AMVP + 1/4 像素插值（8-tap/4-tap）
//            + 可选双参考 L0=[POC-1,POC-2]（hevc_multi_ref）。整数/亚像素/多参考
//            三条路径均经 ffmpeg 真解码 recon==dec 门禁，默认开关保字节级基线。
#ifndef TERMCHART_IMAGE_HEVC_ENCODE_HPP
#define TERMCHART_IMAGE_HEVC_ENCODE_HPP

#include <span>
#include <string>

#include "termchart/export.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/encode.hpp"

namespace termchart::image {

// 按 EncodeOptions::video_mode 分派档位。默认 Pcm：输出完整独立的 I_PCM 码流
// （VPS + SPS + PPS + 每帧一个 IDR slice），字节级稳定，供 diag_hevc.py 做黄金回归。
// Intra 与 Inter 均已落地；不支持的档位组合显式报错，不静默降级。
TERMCHART_API std::string encode_hevc_annexb(std::span<const Bitmap> frames, const EncodeOptions& opts = {});

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_HEVC_ENCODE_HPP
