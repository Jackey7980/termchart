// av1_encode.hpp —— AV1 一期最小纵切：8x8 单帧 IVF（内部接口，尚未接入公开分发）
#ifndef TERMCHART_SRC_IMAGE_AV1_ENCODE_HPP
#define TERMCHART_SRC_IMAGE_AV1_ENCODE_HPP

#include <array>
#include <cstdint>
#include <string>

#include "termchart/image/bitmap.hpp"

namespace termchart::image {

// 8x8 4:2:0 的期望解码画面；色度在本期固定为中性灰。
struct Av1MinimalYuv {
    std::array<std::uint8_t, 64> y{};
    std::array<std::uint8_t, 16> u{};
    std::array<std::uint8_t, 16> v{};
};

// 目前只接受 8x8 的 RGB/RGBA Bitmap；其它尺寸返回空串（fail-loud 由调用方处理）。
// expected 非空时写入与编码器同一条反量化 / 反变换路径算出的期望像素。
std::string encode_av1_minimal_ivf(const Bitmap& frame, Av1MinimalYuv* expected = nullptr);

}  // namespace termchart::image

#endif  // TERMCHART_SRC_IMAGE_AV1_ENCODE_HPP
