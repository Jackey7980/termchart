// quantize.cpp —— HEVC 4x4 量化 / 逆量化
//
// 逆量化逐常量对齐 ffmpeg libavcodec/hevc/cabac.c：
//   deq = (level * scale * scale_m + add) >> shift
// 其中 scale = level_scale[qp%6] << (qp/6)、scale_m = 16（无缩放矩阵）、
// shift = bit_depth + log2_trafo_size - 5（8bit 4x4 → 5）、add = 1 << (shift-1)。
// 前向即该式的舍入逆：level = round(coeff * 2^shift / (scale * scale_m))。
#include "quantize.hpp"

#include <algorithm>
#include <cmath>

namespace termchart::image::codec::hevc {
namespace {

// ffmpeg level_scale[]（HEVC 8.5.6 前 6 项）。
constexpr std::uint8_t kLevelScale[6] = {40, 45, 51, 57, 64, 72};
constexpr int kBitDepth = 8;
constexpr int kLog2TrafoSize = 2;                       // 4x4
constexpr int kShift = kBitDepth + kLog2TrafoSize - 5;  // = 5
constexpr int kScaleM = 16;                             // 无缩放矩阵

}  // namespace

int quant_4x4(const std::int32_t* coeff, std::int16_t* level, int qp) {
    qp = std::clamp(qp, 0, 51);
    const int rem = qp % 6;
    const int div = qp / 6;
    // 逆量化分母：deq ≈ level * (scale * scale_m) / 2^shift = level * den / 2
    // 故 level ≈ coeff * 2 / den。
    const std::int64_t den = static_cast<std::int64_t>(kLevelScale[rem]) << div;
    int nonzero = 0;
    for (int k = 0; k < 16; ++k) {
        const std::int64_t a = std::abs(static_cast<std::int64_t>(coeff[k]));
        const std::int64_t v = (a * 2 + den / 2) / den;
        const std::int64_t m = std::min<std::int64_t>(v, 32767);
        level[k] = static_cast<std::int16_t>(coeff[k] < 0 ? -m : m);
        nonzero += (v != 0) ? 1 : 0;
    }
    return nonzero;
}

void dequant_4x4(const std::int16_t* level, std::int32_t* coeff, int qp) {
    qp = std::clamp(qp, 0, 51);
    const int rem = qp % 6;
    const int div = qp / 6;
    const std::int64_t scale = static_cast<std::int64_t>(kLevelScale[rem]) << div;
    const std::int64_t add = static_cast<std::int64_t>(1) << (kShift - 1);
    for (int k = 0; k < 16; ++k) {
        const std::int64_t dq =
            (static_cast<std::int64_t>(level[k]) * scale * kScaleM + add) >> kShift;
        coeff[k] = static_cast<std::int32_t>(std::clamp<std::int64_t>(dq, -32768, 32767));
    }
}

double quant_step(int qp) {
    // 有效步长（变换域 deq/level）= scale*scale_m/2^shift = den / 2。
    qp = std::clamp(qp, 0, 51);
    const int rem = qp % 6;
    const int div = qp / 6;
    return std::ldexp(static_cast<double>(kLevelScale[rem]), div) / 2.0;
}

}  // namespace termchart::image::codec::hevc
