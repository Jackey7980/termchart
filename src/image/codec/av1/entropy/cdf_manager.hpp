// cdf_manager.hpp —— AV1 多符号 CDF 编码（15 位 CDF → 8 位概率，静态表）
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_ENTROPY_CDF_MANAGER_HPP
#define TERMCHART_SRC_IMAGE_CODEC_AV1_ENTROPY_CDF_MANAGER_HPP

#include <cstdint>

#include "daala_engine.hpp"

namespace termchart::image::codec::av1 {

// 一元 CDF 分解：对每个 i<n-1 发 (symbol > i)，概率取 cdf[i] >> 7。
void write_symbol(DaalaEncoder& enc, const std::uint16_t* cdf, int n, int symbol);
bool read_symbol(DaalaDecoder& dec, const std::uint16_t* cdf, int n, int& symbol);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_ENTROPY_CDF_MANAGER_HPP
