// rdo.hpp —— AV1 阶段八：码率-失真决策核心与提前终止。
// 码率来自 CDF 自信息；失真用 SSD；决策遍历候选模式并可用下界提前剪枝。
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_RDO_RDO_HPP_
#define TERMCHART_SRC_IMAGE_CODEC_AV1_RDO_RDO_HPP_

#include <cstddef>
#include <cstdint>

namespace termchart::image::codec::av1 {

// 16-bit CDF（32768 总量）中 symbol 的自信息位数。
double av1_rdo_symbol_bits(const std::uint16_t* cdf, int n_symbols, int symbol);

// 残差系数的粗略码率：按非零个数 + 幅值 log2 计，量化后调用。
double av1_rdo_residual_bits(const std::int16_t* coeff, int n);

// 8-bit 平面 SSD（以平方和计，不做归一化）。
std::uint64_t av1_rdo_sse(const std::uint8_t* a, const std::uint8_t* b, int n);

struct RdoModeResult {
    int mode = -1;
    std::uint64_t distortion = 0;
    double rate = 0;
    double cost = 0;
};

// candidates[k] 是第 k 个候选模式的 8x8 预测（k*64 起，逐行主序）。
// mode_cdf 是模式符号 CDF；lambda 为 RD 权重。early_exit=true 时若当前
// 累计失真已超过目前最优成本就跳过剩余样点（结果与非提前终止一致）。
RdoModeResult av1_rdo_pick_mode(const std::uint8_t* src,
                                const std::uint8_t* candidates,
                                int n_candidates, const std::uint16_t* mode_cdf,
                                double lambda, bool early_exit);

// 划分决策：两个子块预测 vs 整块预测的 RD 比较；返回 true 表示应继续划分。
bool av1_rdo_should_split(const std::uint8_t* src, const std::uint8_t* whole,
                          const std::uint8_t* parts, int n, double lambda);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_RDO_RDO_HPP_
