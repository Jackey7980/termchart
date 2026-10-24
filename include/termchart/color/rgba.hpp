// rgba.hpp —— 带透明度的颜色参数包（header-only，属 cmake/header_only.txt）
//
// 分层：这是 L2 color 层的"管线中间态"，不是底层像素格式。core::Rgb 永远只有
// 3 字节，alpha 只活在这里与调用方手里；压平（blend）之后才落回 Rgb 上屏。
//
// 为什么 blend 在这里而不是 math：math 是 L1，不能 include L2 的 Rgba。把具体
// 业务类型当参数传给 math 就是分层违规，所以混合留在这层。
#ifndef TERMCHART_COLOR_RGBA_HPP
#define TERMCHART_COLOR_RGBA_HPP

#include <cstdint>

#include "termchart/core/rgb.hpp"

namespace termchart::color {

struct Rgba {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    constexpr Rgba() noexcept = default;
    constexpr Rgba(std::uint8_t red, std::uint8_t green, std::uint8_t blue,
                   std::uint8_t alpha = 255) noexcept
        : r(red), g(green), b(blue), a(alpha) {}

    // 隐式：不透明色是 Rgba 的子集，升级时 alpha = 255 没有信息损失。
    constexpr Rgba(const Rgb& c) noexcept : r(c.r), g(c.g), b(c.b), a(255) {}

    // 显式：丢弃 alpha 是**有损**的（半透明会变成全不透明），所以必须写出来。
    [[nodiscard]] constexpr Rgb to_rgb() const noexcept { return Rgb{r, g, b}; }

    constexpr bool operator==(const Rgba&) const noexcept = default;
    constexpr bool operator!=(const Rgba&) const noexcept = default;
};

// over 合成：src 叠在 dst 之上，返回压平后的不透明色。
//   out = src·a + dst·(1−a)
// 整数实现（+127 四舍五入），a=0 精确得 dst、a=255 精确得 src；无浮点、无分支。
constexpr Rgb blend(const Rgba& src, const Rgb& dst) noexcept {
    const auto channel = [alpha = src.a](std::uint8_t s, std::uint8_t d) constexpr {
        const unsigned inv = 255u - alpha;
        return static_cast<std::uint8_t>(
            (static_cast<unsigned>(s) * alpha + static_cast<unsigned>(d) * inv + 127u) / 255u);
    };
    return Rgb{channel(src.r, dst.r), channel(src.g, dst.g), channel(src.b, dst.b)};
}

}  // namespace termchart::color

#endif  // TERMCHART_COLOR_RGBA_HPP
