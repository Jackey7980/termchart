// png.hpp —— PNG 解码（8/16 位、非隔行；输出 RGBA）
//
// 与 png_encode.hpp 对称：编码器自带（零依赖），解码器也自带——同样是为了不把
// zlib 变成硬依赖。解码器包含一个完整的 **inflate**（stored / 固定 Huffman /
// 动态 Huffman 三种块），因此能读任意常规 PNG，而不只是本库自己写出的那种。
//
// 支持范围与刻意的限制（都在返回值里明确报错，不静默出错图）：
//   * 位深：1 / 2 / 4 / 8 / 16；
//   * 颜色类型：0 灰度、2 真彩、3 调色板、4 灰度+alpha、6 真彩+alpha；
//   * tRNS：调色板透明与"单一透明色"（灰度/真彩）都处理；
//   * **不支持隔行（Adam7）**：返回 `ErrorCode::Unsupported`；
//   * 16 位样本**取高字节**降为 8 位（终端与 GUI 都用不到 16 位深度）；
//   * 每个 chunk 的 CRC 都要对，错一个即返回 `ErrorCode::ParseError`。
// 输出恒为 4 通道 RGBA（调用方要 RGB 用 `Bitmap::without_alpha()`）。
#ifndef TERMCHART_IMAGE_PNG_HPP
#define TERMCHART_IMAGE_PNG_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"
#include "termchart/image/bitmap.hpp"

namespace termchart::image {

// 单张 PNG 的原始扫描线解压后不得大于此值（防止畸形头导致天量分配）。
inline constexpr std::uint64_t kPngMaxRawBytes = 512ull * 1024ull * 1024ull;

[[nodiscard]] TERMCHART_API Result<Bitmap> decode_png(const std::uint8_t* data,
                                                      std::size_t size);

[[nodiscard]] TERMCHART_API inline Result<Bitmap> decode_png(std::string_view bytes) {
    return decode_png(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
}

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_PNG_HPP
