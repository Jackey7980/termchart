// jpeg_encode.hpp —— 最小 baseline JPEG 编码器（8 位，3 通道 YCbCr 4:4:4，无隔行/无算术编码）
//
// 与 png_encode 同哲学：零外部依赖，自带 FDCT / 量化 / 霍夫曼，输出合规 JFIF 流，
// 任何解码器（stb_image / Pillow / 浏览器）都能读。范围刻意最小：
//   * 顺序（sequential）baseline，非渐进（progressive）；
//   * 哈夫曼（Huffman）编码，非算术（arithmetic）编码；
//   * 3 个分量 Y/Cb/Cr，4:4:4 不子采样（最简单且不失色）；
//   * 标准量化表 + 标准 DC/AC 哈夫曼表（JPEG 规范 Annex K），按 quality 缩放。
// 颜色：先 RGB → YCbCr（BT.601），每个 8×8 块独立编码；图像尺寸补到 8 的倍数。
#ifndef TERMCHART_IMAGE_JPEG_ENCODE_HPP
#define TERMCHART_IMAGE_JPEG_ENCODE_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "termchart/export.hpp"
#include "termchart/image/bitmap.hpp"

namespace termchart::image {

// 编码为完整 JPEG 字节流。quality ∈ [1,100]（默认 85）；越高质量越好、体积越大。
// 空位图（宽或高为 0）返回空串；不产出非法文件。
TERMCHART_API std::string encode_jpeg(const Bitmap& bitmap, int quality = 85);

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_JPEG_ENCODE_HPP
