// encode.hpp —— 统一位图编码分发器（自包含编码器入口）
//
// 与现有 encode_png / encode_jpeg 同哲学：零外部依赖，每个编码器把 Bitmap 编码成
// 完整文件字节流（std::string）。本文件只提供一个按格式分流的薄入口，新增格式
// （WebP / GIF / MP4 / AVIF）只需：① 在 ImageFormat 追加枚举值；② 实现对应的
// encode_* 函数（返回字节流）；③ 在 encode() 里加一个 case。既有 encode_png /
// encode_jpeg 保持不变，对外契约不动。
#ifndef TERMCHART_IMAGE_ENCODE_HPP
#define TERMCHART_IMAGE_ENCODE_HPP

#include <string>

#include "termchart/export.hpp"
#include "termchart/image/bitmap.hpp"

namespace termchart::image {

// 支持的位图导出格式。顺序无关；新增格式追加到末尾即可。
enum class ImageFormat {
    Png,
    Jpeg,
    // Webp / Gif / Mp4 / Avif 在各自编码器落地后追加
};

// 编码选项。目前只有 quality；后续有损/无损差异可在此扩展。
struct EncodeOptions {
    int quality = 85;  // 有损格式（JPEG / WebP / MP4）质量 [1,100]；无损格式忽略。
};

// 按 format 把位图编码成完整文件字节流。
// 与 encode_png / encode_jpeg 保持一致：空位图返回空串；未知格式也返回空串
// （调用方应先用 capability 判断，或依赖返回的空串判定失败）。
TERMCHART_API std::string encode(const Bitmap& bitmap, ImageFormat format,
                                const EncodeOptions& opts = {});

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_ENCODE_HPP
