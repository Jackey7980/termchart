// bit_ops.hpp —— 无状态、零格式依赖的基础整数原语。
//
// 只接受基础整数类型，返回基础整数类型；不持有 Context / State / CDF，
// 不接收 AV1Block / HEVCBlock 之类的编码器专属结构。AV1 与 HEVC 的像素限幅
// 语义在此统一为同一份实现，语法层仍各自保留调用点，避免改动码流基线。
#ifndef TERMCHART_SRC_MEDIA_ENCODER_GPA_BIT_OPS_HPP
#define TERMCHART_SRC_MEDIA_ENCODER_GPA_BIT_OPS_HPP

#include <cstdint>

namespace termchart::media::encoder::gpa {

constexpr int clip3(int lo, int hi, int v) noexcept {
    return v < lo ? lo : (v > hi ? hi : v);
}

constexpr int clip_pixel(int v) noexcept { return clip3(0, 255, v); }

// 把低 ln 位按位反转（VP8L 码字在流中以反转序发射）。
constexpr std::uint32_t reverse_bits(std::uint32_t c, int ln) noexcept {
    std::uint32_t r = 0;
    for (int i = 0; i < ln; ++i) r = (r << 1) | ((c >> i) & 1u);
    return r;
}

}  // namespace termchart::media::encoder::gpa

#endif  // TERMCHART_SRC_MEDIA_ENCODER_GPA_BIT_OPS_HPP
