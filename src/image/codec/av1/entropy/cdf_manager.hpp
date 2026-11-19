// cdf_manager.hpp —— 多符号 CDF 编码 / 解码 / 自适应更新
//
// cdf 采用 Q15 逆累积表：cdf[0..n-1] 单调递减，cdf[n-1] == 0（最后一个符号没有下界）。
// 若表在 cdf[n] 处还有一槽，则用作自适应计数（0..32），与规范一致。
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_ENTROPY_CDF_MANAGER_HPP
#define TERMCHART_SRC_IMAGE_CODEC_AV1_ENTROPY_CDF_MANAGER_HPP

#include <cstdint>

#include "range_coder.hpp"

namespace termchart::image::codec::av1 {

// 写入一个符号；cdf 为逆累积表，n 为符号数。
void write_symbol(RangeEncoder& enc, const std::uint16_t* cdf, int n, int symbol);

// 解码一个符号；返回 [0, n) 内的下标。
unsigned read_symbol(RangeDecoder& dec, const std::uint16_t* cdf, int n);

// 编码 / 解码成功后的自适应更新（rate 随计数增长而降低）。
void update_cdf(std::uint16_t* cdf, int n, unsigned symbol);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_ENTROPY_CDF_MANAGER_HPP
