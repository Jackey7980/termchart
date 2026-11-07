// transquant.hpp —— 阶段一 B2：残差 / 重建链路
//
// 组合 transform + quantize：encode_tu_4x4 输出量化 level（供 cbf 与残差 CABAC
// 语法使用），同时把重建像素写回 rec，保证编码端预测基准与解码端一致。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_TRANSQUANT_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_TRANSQUANT_HPP

#include <cstdint>

namespace termchart::image::codec::hevc {

// src / rec 为 8bit 平面（各自 stride）；pred 为 4x4 行优先预测块。
// 返回非零 level 个数，0 表示 cbf=0（无需写残差）。
int encode_tu_4x4(const std::uint8_t* src, int src_stride,
                  const std::uint8_t* pred, std::uint8_t* rec, int rec_stride,
                  std::int16_t* level, int qp);

// 仅反量化 + 逆变换，把残差加到 pred 上写回 rec（解码端等价路径）。
void reconstruct_tu_4x4(const std::int16_t* level, int qp,
                        const std::uint8_t* pred, std::uint8_t* rec,
                        int rec_stride);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_TRANSQUANT_HPP
