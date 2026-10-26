// color_space.hpp —— 色彩空间换算（header-only，属 cmake/header_only.txt）
//
// 分层铁律：math 层只进标量与 core::Rgb，绝不 include color 层。所以这里的
// "中间态"（HSV 的 h/s/v、Lab 的 L/a/b）一律走引用出参，不在本文件定义结构体；
// 结构体（Rgba / Hsv）属 color 层，那是给用户与渲染管线用的参数包。
//
// 关于 constexpr：hsv_to_rgb / rgb_to_hsv 全常量求值；sRGB 与 Lab 那条链要吃
// std::pow（C++20 非 constexpr），因此只能是普通 inline 函数。别为了"全 constexpr"
// 把它们硬塞进常量求值——那会重犯"近似代替真值"的错。
#ifndef TERMCHART_MATH_COLOR_SPACE_HPP
#define TERMCHART_MATH_COLOR_SPACE_HPP

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "termchart/core/rgb.hpp"
#include "termchart/math/power.hpp"

namespace termchart::math {

// ── sRGB 电光转换（IEC 61966-2-1）────────────────────────────────────────────
// 暗部（≤0.04045）是斜率 12.92 的线性段，亮部是 1.055·x^2.4 − 0.055。
// 两段的分界与缩放必须成对出现，少一个就得到"颜色略偏暗"的经典 bug。

inline double srgb_to_linear(double c) noexcept {
    if (c <= 0.04045) {
        return c / 12.92;  // 负值沿用同一线性段，交给调用方决定是否钳位
    }
    return std::pow((c + 0.055) / 1.055, 2.4);
}

inline double linear_to_srgb(double x) noexcept {
    if (x <= 0.0031308) {
        return 12.92 * x;
    }
    return 1.055 * std::pow(x, 1.0 / 2.4) - 0.055;
}

// 8bit 输入专用查表：像素级批量转线性时，pow 是热点。
// 只覆盖"8bit 采样值 → 线性 float"这一个方向，语义就是完整的 sRGB EOTF
// （含线性段与 1.055/0.055 缩放）——别把它当纯 x^2.4 表用。
// 函数内 static：C++11 起线程安全的唯一实例，不会被每个 TU 复制一份。
inline const std::array<float, 256>& srgb8_to_linear_table() noexcept {
    static const std::array<float, 256> table = [] {
        std::array<float, 256> t{};
        for (std::size_t i = 0; i < t.size(); ++i) {
            t[i] = static_cast<float>(srgb_to_linear(static_cast<double>(i) / 255.0));
        }
        return t;
    }();
    return table;
}

inline float srgb8_to_linear(std::uint8_t c) noexcept { return srgb8_to_linear_table()[c]; }

inline std::uint8_t linear_to_srgb8(double x) noexcept {
    const double v = linear_to_srgb(x) * 255.0 + 0.5;
    if (!(v > 0.0)) {  // NaN 与负值都落到 0
        return 0;
    }
    if (v >= 255.0) {
        return 255;
    }
    return static_cast<std::uint8_t>(v);
}

// ── HSV ↔ RGB ───────────────────────────────────────────────────────────────
// h ∈ [0,360)（自动取模），s / v ∈ [0,1]（钳位）。返回量化后的 Rgb。

namespace detail {

constexpr double clamp01(double x) noexcept {
    return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
}

// 取模 360。NaN / 超大量级（|h| ≥ 1e6，long long 截断会溢出）一律归 0：
// 色相本来就是循环量，把不可解释的输入钉在红端比返回 NaN 更能用。
constexpr double normalize_hue(double h) noexcept {
    if (!(h > -1.0e6 && h < 1.0e6)) {
        return 0.0;
    }
    double x = h - 360.0 * static_cast<double>(static_cast<long long>(h / 360.0));
    if (x < 0.0) {
        x += 360.0;
    }
    if (x >= 360.0) {
        x -= 360.0;
    }
    return x;
}

constexpr std::uint8_t to_byte(double x) noexcept {
    return static_cast<std::uint8_t>(clamp01(x) * 255.0 + 0.5);
}

constexpr double absd(double v) noexcept {
    return v < 0.0 ? -v : v;
}

}  // namespace detail

constexpr Rgb hsv_to_rgb(double h, double s, double v) noexcept {
    const double hue = detail::normalize_hue(h);
    const double sat = detail::clamp01(s);
    const double val = detail::clamp01(v);
    const double c = val * sat;
    const double hp = hue / 60.0;  // ∈ [0,6)
    // 第二高分量：x = c·(1 − |hp mod 2 − 1|)，即色环上"升降沿"的三角波。
    const double mod2 = hp - 2.0 * static_cast<double>(static_cast<int>(hp / 2.0));
    const double x = c * (1.0 - (mod2 < 1.0 ? 1.0 - mod2 : mod2 - 1.0));
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    if (hp < 1.0) {
        r = c;
        g = x;
    } else if (hp < 2.0) {
        r = x;
        g = c;
    } else if (hp < 3.0) {
        g = c;
        b = x;
    } else if (hp < 4.0) {
        g = x;
        b = c;
    } else if (hp < 5.0) {
        r = x;
        b = c;
    } else {
        r = c;
        b = x;
    }
    const double m = val - c;
    return Rgb{detail::to_byte(r + m), detail::to_byte(g + m), detail::to_byte(b + m)};
}

// 出参版本：math 层不定义 Hsv 结构体（那属 color 层）。
// 纯灰（s = 0）时 h 无定义，按惯例返回 0。
constexpr void rgb_to_hsv(Rgb c, double& h, double& s, double& v) noexcept {
    const double r = static_cast<double>(c.r) / 255.0;
    const double g = static_cast<double>(c.g) / 255.0;
    const double b = static_cast<double>(c.b) / 255.0;
    const double mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    const double mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    const double d = mx - mn;
    v = mx;
    s = mx > 0.0 ? d / mx : 0.0;
    if (d <= 0.0) {
        h = 0.0;
        return;
    }
    if (mx == r) {
        h = 60.0 * ((g - b) / d);
    } else if (mx == g) {
        h = 60.0 * (2.0 + (b - r) / d);
    } else {
        h = 60.0 * (4.0 + (r - g) / d);
    }
    if (h < 0.0) {
        h += 360.0;
    }
}

// ── HSL（HSV 同族，lightness 版；"调亮/调暗"类配色更符合直觉）──────────────
constexpr Rgb hsl_to_rgb(double h, double s, double l) noexcept {
    const double hue = detail::normalize_hue(h);
    const double sat = detail::clamp01(s);
    const double light = detail::clamp01(l);
    const double c = (1.0 - detail::absd(2.0 * light - 1.0)) * sat;
    const double hp = hue / 60.0;  // ∈ [0,6)
    const double mod2 = hp - 2.0 * static_cast<double>(static_cast<int>(hp / 2.0));
    const double x = c * (1.0 - (mod2 < 1.0 ? 1.0 - mod2 : mod2 - 1.0));
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    if (hp < 1.0) {
        r = c;
        g = x;
    } else if (hp < 2.0) {
        r = x;
        g = c;
    } else if (hp < 3.0) {
        g = c;
        b = x;
    } else if (hp < 4.0) {
        g = x;
        b = c;
    } else if (hp < 5.0) {
        r = x;
        b = c;
    } else {
        r = c;
        b = x;
    }
    const double m = light - c * 0.5;
    return Rgb{detail::to_byte(r + m), detail::to_byte(g + m), detail::to_byte(b + m)};
}

constexpr void rgb_to_hsl(Rgb c, double& h, double& s, double& l) noexcept {
    const double r = static_cast<double>(c.r) / 255.0;
    const double g = static_cast<double>(c.g) / 255.0;
    const double b = static_cast<double>(c.b) / 255.0;
    const double mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    const double mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    const double d = mx - mn;
    l = (mx + mn) * 0.5;
    if (d <= 0.0) {
        h = 0.0;
        s = 0.0;
        return;
    }
    const double denom = 1.0 - detail::absd(2.0 * l - 1.0);
    s = denom > 0.0 ? d / denom : 0.0;
    if (mx == r) {
        h = 60.0 * ((g - b) / d);
    } else if (mx == g) {
        h = 60.0 * (2.0 + (b - r) / d);
    } else {
        h = 60.0 * (4.0 + (r - g) / d);
    }
    if (h < 0.0) {
        h += 360.0;
    }
}

// ── CIE Lab（D65）────────────────────────────────────────────────────────────
// 储备能力：色差比较、感知均匀插值将来会用到。整条链含 std::pow，故非 constexpr。

namespace detail {

inline constexpr double kLabEpsilon = 216.0 / 24389.0;  // (6/29)^3
inline constexpr double kLabKappa = 24389.0 / 27.0;     // (29/3)^3

constexpr double lab_f(double t) noexcept {
    return t > kLabEpsilon ? cbrt(t) : (kLabKappa * t + 16.0) / 116.0;
}

constexpr double lab_f_inv(double t) noexcept {
    const double t3 = t * t * t;
    return t3 > kLabEpsilon ? t3 : (116.0 * t - 16.0) / kLabKappa;
}

}  // namespace detail

inline void rgb_to_lab(Rgb c, double& l, double& a, double& b) noexcept {
    const double r = srgb_to_linear(static_cast<double>(c.r) / 255.0);
    const double g = srgb_to_linear(static_cast<double>(c.g) / 255.0);
    const double bl = srgb_to_linear(static_cast<double>(c.b) / 255.0);
    // sRGB → XYZ（D65 白点）
    const double x = 0.4124564 * r + 0.3575761 * g + 0.1804375 * bl;
    const double y = 0.2126729 * r + 0.7151522 * g + 0.0721750 * bl;
    const double z = 0.0193339 * r + 0.1191920 * g + 0.9503041 * bl;
    const double fx = detail::lab_f(x / 0.95047);
    const double fy = detail::lab_f(y / 1.0);
    const double fz = detail::lab_f(z / 1.08883);
    l = 116.0 * fy - 16.0;
    a = 500.0 * (fx - fy);
    b = 200.0 * (fy - fz);
}

inline Rgb lab_to_rgb(double l, double a, double b) noexcept {
    const double fy = (l + 16.0) / 116.0;
    const double fx = fy + a / 500.0;
    const double fz = fy - b / 200.0;
    const double x = 0.95047 * detail::lab_f_inv(fx);
    const double y = detail::lab_f_inv(fy);
    const double z = 1.08883 * detail::lab_f_inv(fz);
    // XYZ → linear sRGB
    const double r = 3.2404542 * x - 1.5371385 * y - 0.4985314 * z;
    const double g = -0.9692660 * x + 1.8760108 * y + 0.0415560 * z;
    const double bl = 0.0556434 * x - 0.2040259 * y + 1.0572252 * z;
    return Rgb{linear_to_srgb8(r), linear_to_srgb8(g), linear_to_srgb8(bl)};
}

}  // namespace termchart::math

#endif  // TERMCHART_MATH_COLOR_SPACE_HPP
