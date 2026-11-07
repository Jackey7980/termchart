// residual_coding.hpp —— 阶段一 B3：4x4 TU 残差语法的 bin 序列
//
// 只负责「量化系数 -> CABAC bin 序列」；bin -> bits 由 cabac_arith 完成。
// bin 序列与算术层解耦，便于在接真码流前逐字段核对。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_RESIDUAL_CODING_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_RESIDUAL_CODING_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "cabac_syntax.hpp"  // Bin / kBypassCtx 由语法层统一提供

namespace termchart::image::codec::hevc {

// 4x4 TU 的 HEVC 对角扫描表（scan position -> (x,y)）及反查表。
extern const std::uint8_t kDiagScan4x4X[16];
extern const std::uint8_t kDiagScan4x4Y[16];
extern const std::uint8_t kDiagScan4x4Inv[4][4];
// log2_trafo_size==2 / SCAN_DIAG 的 significant_coeff_flag ctx 映射。
extern const std::uint8_t kSigCoeffCtxMap4x4[16];

// coeff[16] 按 4x4 对角扫描序存放量化 level（含符号）。
// 全零时返回空序列（调用方应写 cbf=0，不进入 residual_coding）。
//
// 当前子集：亮度 c_idx=0、log2_trafo_size=2、SCAN_DIAG、单 CG、
// sign_data_hiding_flag=0、无 transform_skip / scaling_list。
// Rice 的 escape 分支（prefix>=3）暂不支持，见实现中的断言。
// ok 为 false 表示遇到尚未实现的 Rice escape 分支（返回空序列）。
std::vector<Bin> encode_residual_4x4(const std::int16_t* coeff, bool* ok = nullptr);

// 按 FFmpeg 解码语义独立回读，用于自检与回归；成功时写回 coeff 与消耗 bin 数。
bool decode_residual_4x4(const Bin* bins, std::size_t count, std::int16_t* coeff,
                         std::size_t* consumed);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_RESIDUAL_CODING_HPP
