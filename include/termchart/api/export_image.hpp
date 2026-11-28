// export_image.hpp —— 位图导出高层入口（chart → 文件）
//
// 这是「把一张图存成文件」的统一门面：编码（image::encode）+ 写盘（api::write_to）
// 两步合一。chart / 应用层只调这一个函数即可，不必关心各编码器细节。所有格式经同一
// ImageFormat 枚举选择，避免散落一堆 save_png / save_jpeg 游离函数。
#ifndef TERMCHART_API_EXPORT_IMAGE_HPP
#define TERMCHART_API_EXPORT_IMAGE_HPP

#include <span>
#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/encode.hpp"

namespace termchart::api {

// 编码位图并写入 path（二进制）。返回 Status（= ErrorCode）：
//   Ok            成功写盘；
//   InvalidArgument  位图为空或对应格式未实现（encode 返回空串）；
//   IoError       文件打不开或写失败。
TERMCHART_API Status save_image(const image::Bitmap& bitmap, std::string_view path,
                               image::ImageFormat format,
                               const image::EncodeOptions& opts = {});

// 多帧重载（动画 GIF / 视频 MP4 / AVIF）：把一串 Bitmap 编码并写入 path。
// 单帧调用等价于把 frames 设为 1 张；视频容器经 opts.video_codec 选 H264/H265。
TERMCHART_API Status save_image(std::span<const image::Bitmap> frames, std::string_view path,
                               image::ImageFormat format,
                               const image::EncodeOptions& opts = {});

}  // namespace termchart::api

#endif  // TERMCHART_API_EXPORT_IMAGE_HPP
