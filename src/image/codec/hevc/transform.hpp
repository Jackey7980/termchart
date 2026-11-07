// transform.hpp —— 阶段一 B1：4x4 整数变换接口
//
// 与 quantize 的 MF / levelScale 表成对使用：fwd 输出未归一化的核心变换系数，
// inv 接收反量化后的系数并含最终 (x+32)>>6 归一化。纯整数运算，无 HEVC 语法。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_TRANSFORM_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_TRANSFORM_HPP

#include <cstdint>

namespace termchart::image::codec::hevc {

// src/dst 均为行优先 idx = y*4 + x。
void fwd_transform_4x4(const std::int16_t* residual, std::int32_t* coeff);
void inv_transform_4x4(const std::int32_t* coeff, std::int16_t* residual);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_TRANSFORM_HPP
