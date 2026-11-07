// intra_prediction.hpp —— HEVC 4x4 帧内预测（Planar / DC）
//
// 参考样本：top[0..n-1] 为上方列 0..n-1，top[n] 为右上；left[0..n-1] 为左侧
// 行 0..n-1，left[n] 为左下。has_top / has_left 指示参考侧是否可用（边界外推）。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_INTRA_PREDICTION_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_INTRA_PREDICTION_HPP

#include <cstdint>

namespace termchart::image::codec::hevc {

// mode: 0 = Planar, 1 = DC（其余角度模式暂未实现，按 DC 处理）。
// dst 为 n*n 行优先预测块。
void predict_intra(int mode, int n, const std::uint8_t* top,
                   const std::uint8_t* left, bool has_top, bool has_left,
                   std::uint8_t* dst);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_INTRA_PREDICTION_HPP
