// webp_encode.hpp —— 自包含 VP8L lossless WebP 编码器（零依赖）
//
// 与 png_encode / jpeg_encode / gif_encode 同哲学：把 Bitmap 编码成完整文件字节流，
// 不引入任何外部依赖。当前实现 v1：
//   · 单帧无损（无变换 / 无色彩缓存 / 平铺 8-bit 霍夫曼码 + 字面量像素）；
//   · 编码后字节流逐字节可由 ffmpeg 解码（codec=webp, 子流 vp8l）。
// 后续增强（独立任务，不阻塞 v1 正确性）：频率自适应霍夫曼、LZ77 反向引用、
// VP8X/ANIM 多帧动画、有损 VP8。
#ifndef TERMCHART_IMAGE_WEBP_ENCODE_HPP
#define TERMCHART_IMAGE_WEBP_ENCODE_HPP

#include <span>
#include <string>

#include "termchart/export.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/encode.hpp"

namespace termchart::image {

// 编码一帧（或多帧取首帧）为 WebP。frames 为空 / 首帧尺寸为 0 / 超出 14-bit
// 尺寸上限（16383）时返回空串（调用方据 encode() 返回空判定 Unsupported）。
TERMCHART_API std::string encode_webp(std::span<const Bitmap> frames, const EncodeOptions& opts = {});

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_WEBP_ENCODE_HPP
