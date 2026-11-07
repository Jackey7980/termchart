// intra_prediction.hpp —— 阶段一 B2：帧内预测（DC / Planar / V / H）
//
// 头只暴露模式枚举与接口；参考样本填充与角度模式（35 种）后续扩展。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_INTRA_PREDICTION_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_INTRA_PREDICTION_HPP

#include <cstdint>

namespace termchart::image::codec::hevc {

enum class IntraMode : std::uint8_t {
    Dc = 0,
    Planar = 1,
    Vertical = 2,
    Horizontal = 3,
};

// 把不可用的参考侧按 HEVC 8.4.4.2.2 填成 128（或由可用侧外推）。
// top/left 各需 n+1 个样本：top[n] 是右上、left[n] 是左下，供 Planar 使用。
void fill_reference(int n, const std::uint8_t* top, const std::uint8_t* left,
                    bool has_top, bool has_left, std::uint8_t* ref_top,
                    std::uint8_t* ref_left);

// dst 为 n*n 行优先预测块。n 支持 4 / 8 / 16 / 32。
void predict(IntraMode mode, int n, const std::uint8_t* top,
             const std::uint8_t* left, bool has_top, bool has_left,
             std::uint8_t* dst);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_INTRA_PREDICTION_HPP
