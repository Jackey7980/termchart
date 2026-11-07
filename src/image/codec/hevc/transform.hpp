// transform.hpp —— HEVC 4x4 整数变换（DCT-II / DST-VII）
//
// 与 ffmpeg 的 idct 完全同构以保证「编码端重建 == ffmpeg 解码」逐比特一致：
//   - inv_* 直接照搬 ffmpeg libavcodec/hevc/dsp_template.c 的 idct_4x4（DCT-II）
//     与 transform_4x4_luma（DST-VII）的蝶形 + 移位，逐常量对齐。
//   - fwd_* 取对应正向矩阵（HEVC 规范 8.6.2，整数核按 64 缩放），每 1D 段
//     (x+32)>>6 归一化，使其与 inv_* 构成 ~恒等往返（由 _transform_test 验证）。
// 纯整数运算，无 HEVC 语法。行优先 idx = y*4 + x。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_TRANSFORM_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_TRANSFORM_HPP

#include <cstdint>

namespace termchart::image::codec::hevc {

// src/dst 均为行优先 idx = y*4 + x。use_dst=true 用 DST-VII（亮度 4x4 帧内），
// false 用 DCT-II（色度 4x4）。
void fwd_transform_4x4(const std::int16_t* residual, std::int32_t* coeff, bool use_dst);
void inv_transform_4x4(const std::int32_t* coeff, std::int16_t* residual, bool use_dst);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_TRANSFORM_HPP
