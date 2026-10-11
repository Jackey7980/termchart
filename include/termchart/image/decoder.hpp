// decoder.hpp —— 图像解码入口（按魔数识别格式后分发）
//
// 为什么先"识别再分发"：调用方拿到的常常是一段来路不明的字节（剪贴板、文件、
// 网络），先嗅探格式再交给对应解码器，比让每个解码器各自试一遍要清楚。
//
// 目前**只有 PNG 有解码器**；其余格式如实返回 `ErrorCode::Unsupported`，
// 不做"猜着解"这种事。等第二种格式落地时再引入解码器注册表（与 raster/chart
// 同构），现在引入只会是空壳。
#ifndef TERMCHART_IMAGE_DECODER_HPP
#define TERMCHART_IMAGE_DECODER_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"
#include "termchart/image/bitmap.hpp"

namespace termchart::image {

enum class ImageFormat : std::uint8_t {
    Unknown = 0,
    Png,
    Jpeg,
    Gif,
    Bmp,
    WebP,
};

// 只看魔数（不看扩展名）。数据不足或对不上任何魔数返回 Unknown。
[[nodiscard]] TERMCHART_API ImageFormat sniff_image_format(const std::uint8_t* data,
                                                           std::size_t size) noexcept;

[[nodiscard]] TERMCHART_API const char* image_format_name(ImageFormat format) noexcept;

// 识别 + 解码。识别不出返回 `ErrorCode::ParseError`；识别出但无解码器返回
// `ErrorCode::Unsupported`。
[[nodiscard]] TERMCHART_API Result<Bitmap> decode_image(const std::uint8_t* data,
                                                        std::size_t size);

[[nodiscard]] inline Result<Bitmap> decode_image(std::string_view bytes) {
    return decode_image(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
}

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_DECODER_HPP
