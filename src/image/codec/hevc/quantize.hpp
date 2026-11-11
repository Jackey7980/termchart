// quantize.hpp —— HEVC 量化 / 逆量化（支持 4/8/16/32 与 transform_skip）
//
// 逆量化对齐 ffmpeg libavcodec/hevc/cabac.c + dsp_template.c：
//   deq = (level * scale * scale_m + add) >> shift
// 其中 scale = level_scale[qp%6] << (qp/6)、scale_m = 16（默认缩放矩阵全 16）。
// 非 skip：shift = bit_depth + log2_trafo_size - 5，其后接 idct。
// skip：ffmpeg 走两段（cabac.c 段1 >>3+log2、dsp_template.c dequant 段2 >>7-log2，
// 每段各带 clip16 与舍入），净移位 10 与尺寸无关；详见 quantize.cpp。
// 前向为合并舍入逆：level = round(coeff * 2^shift / (scale * scale_m))。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_QUANTIZE_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_QUANTIZE_HPP

#include <cstdint>

namespace termchart::image::codec::hevc {

// QP 合法区间 [0,51]；越界会被夹到边界。返回非零 level 个数。
// log2_size ∈ [2,5]；transform_skip 时按 ffmpeg 两段式逆量化（净移位 10，
// 与尺寸无关），调用方须把残差直接作为 coeff（不做变换）。
int quant(const std::int32_t* coeff, std::int16_t* level, int qp,
          int log2_size, bool transform_skip);
void dequant(const std::int16_t* level, std::int32_t* coeff, int qp,
             int log2_size, bool transform_skip);

// 兼容包装（4x4、非 skip）。
int quant_4x4(const std::int32_t* coeff, std::int16_t* level, int qp);
void dequant_4x4(const std::int16_t* level, std::int32_t* coeff, int qp);

// RD / 码率控制用的名义量化步长（仅供代价估算，不参与码流）。
double quant_step(int qp);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_QUANTIZE_HPP
