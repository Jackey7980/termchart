// gif_encode.hpp —— 最小 GIF89a 编码器（自包含 LZW，支持多帧动画）
//
// 零外部依赖：自带调色板量化（median-cut，≤256 色）+ GIF LZW 压缩，输出合规的
// GIF89a 字节流，任何解码器（ffmpeg / ImageMagick / 浏览器）都能读。多帧 frames
// 经 opts.loop_count / delay_ms 控制循环与帧间隔，单帧退化为静态图；透明通道当前
// 忽略（输出不透明 GIF）。公开接口与 encode_png / encode_jpeg / encode_mp4 同形。
#ifndef TERMCHART_IMAGE_GIF_ENCODE_HPP
#define TERMCHART_IMAGE_GIF_ENCODE_HPP

#include <span>
#include <string>

#include "termchart/export.hpp"
#include "termchart/image/bitmap.hpp"
#include "termchart/image/encode.hpp"

namespace termchart::image {

// 编码为完整 GIF89a 字节流。多帧 → 动画；单帧 → 静态图。空输入或帧尺寸不一致返回空串。
TERMCHART_API std::string encode_gif(std::span<const Bitmap> frames,
                                     const EncodeOptions& opts = {});

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_GIF_ENCODE_HPP
