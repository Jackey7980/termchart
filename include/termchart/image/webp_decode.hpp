#ifndef TERMCHART_IMAGE_WEBP_DECODE_HPP
#define TERMCHART_IMAGE_WEBP_DECODE_HPP

#include <cstddef>
#include <cstdint>

#include "termchart/core/expected.hpp"
#include "termchart/image/bitmap.hpp"

namespace termchart::image {

// 原生零依赖 VP8L lossless WebP 解码。
//
// 仅支持本库编码器（encode_webp）产出的无损字面量流：无 Predictor /
// ColorTransform / SubtractGreen / ColorIndex 变换、无色彩缓存、单一 htree
// group。这些约束与 encode_webp 的 v1 输出一一对应，故可做到像素级精确回环。
//
// 遇到带变换的外部 WebP 文件会返回 ErrorCode::Unsupported —— 调用方
// （decode_image）据此回落到 stb（若构建时启用了 TERMCHART_HAVE_STB_IMAGE）。
// 结构化损坏（容器/签名/尺寸越界）返回 ErrorCode::ParseError。
[[nodiscard]] Result<Bitmap> decode_webp(const std::uint8_t* data,
                                        std::size_t size) noexcept;

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_WEBP_DECODE_HPP
