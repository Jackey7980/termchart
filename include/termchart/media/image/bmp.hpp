// bmp.hpp —— BMP 解码（自主实现，零依赖；输出 RGBA）
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ 文件头 'BM' + BITMAPINFOHEADER（40 字节）与后续更长的 V4/V5 头；
//   ✅ 位深 1 / 4 / 8（调色板，BI_RGB）、24（BGR）、32（BGRX 或 BITFIELDS）、
//      16（BITFIELDS 或默认 RGB555）；
//   ✅ 压缩 0（BI_RGB）与 3（BI_BITFIELDS，带 3~4 个 DWORD 掩码）；
//   ✅ 行序 bottom-up（height 为正）与 top-down（height 为负）；
//   ✅ 行 4 字节对齐、调色板缺省色表（bpp ≤ 8 且 ciColors 为 0 时按 2^bpp 项）；
//   ⛔ 不支持：RLE4/RLE8 压缩（返回 Unsupported）、OS/2 的 12 字节旧头
//      （返回 Unsupported）、每通道超过 8 位的 16bpp 掩码组合（按能处理的
//      掩码解析，掩码不连续/不覆盖时返回 ParseError）。
// 错误：头结构非法/尺寸越界/掩码不可解析 → ParseError；魔数或压缩不支持 →
//      Unsupported。
#ifndef TERMCHART_IMAGE_BMP_HPP
#define TERMCHART_IMAGE_BMP_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"
#include "termchart/media/image/bitmap.hpp"

namespace termchart::image {

// 解码 BMP 为 RGBA。data 至少覆盖文件头声明的全部字节；输出 4 通道，
// 无 alpha 信息的来源位（24bpp）一律填 255。
[[nodiscard]] TERMCHART_API Result<Bitmap> decode_bmp(const std::uint8_t* data,
                                                      std::size_t size);

[[nodiscard]] inline Result<Bitmap> decode_bmp(std::string_view bytes) {
    return decode_bmp(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
}

[[nodiscard]] inline Result<Bitmap> decode_bmp(const std::vector<std::uint8_t>& bytes) {
    return decode_bmp(bytes.data(), bytes.size());
}

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_BMP_HPP
