// quantize.hpp —— 阶段一 B1：QP 量化 / 逆量化接口
//
// 与 transform 的 4x4 核心变换配对：MF 表负责量化，LevelScale 表负责反量化，
// 两者的乘积在同一 QP 下约等于 2^(15+qp/6)，保证往返缩放自洽。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_QUANTIZE_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_QUANTIZE_HPP

#include <cstdint>

namespace termchart::image::codec::hevc {

// QP 合法区间 [0,51]；越界会被夹到边界。返回非零 level 个数。
int quant_4x4(const std::int32_t* coeff, std::int16_t* level, int qp);
void dequant_4x4(const std::int16_t* level, std::int32_t* coeff, int qp);

// RD / 码率控制用的名义量化步长（仅供代价估算，不参与码流）。
double quant_step(int qp);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_QUANTIZE_HPP
