// av1_encode.hpp —— AV1 一期最小纵切：8x8 单帧 IVF（内部接口，尚未接入公开分发）
#ifndef TERMCHART_SRC_IMAGE_AV1_ENCODE_HPP
#define TERMCHART_SRC_IMAGE_AV1_ENCODE_HPP

#include <string>

#include "termchart/image/bitmap.hpp"

namespace termchart::image {

// 目前只接受 8x8 的 RGBA Bitmap；其它尺寸返回空串（fail-loud 由调用方处理）。
std::string encode_av1_minimal_ivf(const Bitmap& frame);

}  // namespace termchart::image

#endif  // TERMCHART_SRC_IMAGE_AV1_ENCODE_HPP
