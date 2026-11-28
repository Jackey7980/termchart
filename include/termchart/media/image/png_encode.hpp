// png_encode.hpp —— 最小 PNG 编码器（8 位 RGB / RGBA，非隔行）
//
// 为什么自带而不是用 libpng/zlib：本仓库的硬约束是"每个外部依赖都必须可缺失"
// （`TERMCHART_ENABLE_PNG` 默认 OFF）。这里只实现 PNG 必需的最小集合：
//
//   * CRC-32（IEEE 802.3）—— 每个 chunk 的校验字段；
//   * Adler-32 —— zlib 流尾校验；
//   * zlib 流里的 **stored（未压缩）deflate 块** —— 不需要字典与霍夫曼编码，
//     体积比压缩版大约多 0.03%，但完全合规，任何解码器都能读。
//
// 将来接入真 deflate（或可选 zlib）时只换内部实现，公开接口不变。
// 代价说清楚：stored 块的输出体积 ≈ 原始像素 + 每 65535 字节 5 字节开销，
// 用于截图/内联图像足够，不适合当压缩格式用。
#ifndef TERMCHART_IMAGE_PNG_ENCODE_HPP
#define TERMCHART_IMAGE_PNG_ENCODE_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "termchart/export.hpp"
#include "termchart/media/image/bitmap.hpp"

namespace termchart::image {

// PNG chunk 用的 CRC-32（标准校验值：crc32("123456789") == 0xCBF43926）。
TERMCHART_API std::uint32_t crc32(const std::uint8_t* data, std::size_t len) noexcept;

// zlib 流尾用的 Adler-32（标准校验值：adler32("123456789") == 0x091E01DE）。
TERMCHART_API std::uint32_t adler32(const std::uint8_t* data, std::size_t len) noexcept;

// 编码为完整 PNG 字节流：签名 + IHDR + IDAT + IEND。
// 颜色类型按位图通道数选（3 → truecolor 2，4 → truecolor+alpha 6），位深恒为 8。
// 宽或高为 0 的空位图返回空串（不产出非法文件）。
TERMCHART_API std::string encode_png(const Bitmap& bitmap);

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_PNG_ENCODE_HPP
