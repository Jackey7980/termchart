// transquant.hpp —— 残差量化与重建（支持 4/8/16/32 与 transform_skip）
//
// 组合 transform + quantize：encode_tu 输出量化 level（供 cbf 与残差 CABAC 语法使用），
// 同时把重建像素写回 rec，保证编码端预测基准与解码端一致。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_TRANSQUANT_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_TRANSQUANT_HPP

#include <cstdint>

namespace termchart::image::codec::hevc {

// src / rec 为 8bit 平面（各自 stride）；pred 为 n×n 行优先预测块（n = 1<<log2_size）。
// use_dst：仅 4x4 亮度帧内有效（DST-VII），其余一律 DCT-II。
// transform_skip：跳过 DCT（残差直接量化/反量化）。
// 返回非零 level 个数，0 表示 cbf=0（无需写残差）。
int encode_tu(const std::uint8_t* src, int src_stride,
              const std::uint8_t* pred, std::uint8_t* rec, int rec_stride,
              std::int16_t* level, int qp, int log2_size, bool use_dst,
              bool transform_skip);

// 仅反量化 + 逆变换，把残差加到 pred 上写回 rec（解码端等价路径）。
void reconstruct_tu(const std::int16_t* level, int qp,
                    const std::uint8_t* pred, std::uint8_t* rec, int rec_stride,
                    int log2_size, bool use_dst, bool transform_skip);

// 兼容包装（4x4、非 skip）。
int encode_tu_4x4(const std::uint8_t* src, int src_stride,
                  const std::uint8_t* pred, std::uint8_t* rec, int rec_stride,
                  std::int16_t* level, int qp, bool use_dst);
void reconstruct_tu_4x4(const std::int16_t* level, int qp,
                        const std::uint8_t* pred, std::uint8_t* rec,
                        int rec_stride, bool use_dst);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_TRANSQUANT_HPP
