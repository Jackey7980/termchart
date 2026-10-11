// math.hpp —— 标量数学（header-only）
//
// 只提供终端绘图真正需要的那几个函数；依赖 <cmath> 但不引入 eigen/boost。
// 所有函数 constexpr / noexcept：布局与缩放是在热路径上跑的。
#ifndef TERMCHART_UTIL_MATH_HPP
#define TERMCHART_UTIL_MATH_HPP

#include <cmath>
#include <limits>
#include <type_traits>

namespace termchart::util {

template <class T>
constexpr T clamp(T v, T lo, T hi) noexcept {
    return v < lo ? lo : (v > hi ? hi : v);
}

template <class T>
constexpr T lerp(T a, T b, T t) noexcept {
    return a + (b - a) * t;
}

// t 钳位到 [0,1] 后的 lerp：绘图 APIs 的边界输入极其常见，默认密封。
template <class T>
constexpr T clamp_lerp(T a, T b, T t) noexcept {
    return lerp<T>(a, b, clamp<T>(t, T{0}, T{1}));
}

// 归一：把 v 从 [lo,hi] 映射到 [0,1]；退化的区间（lo == hi）返回 0，不是 NaN。
constexpr double normalize(double v, double lo, double hi) noexcept {
    const double span = hi - lo;
    if (span == 0.0) {
        return 0.0;
    }
    return (v - lo) / span;
}

// 平滑过渡：比线性少一些"折点感"，用于动画缓动与渐变采样。
constexpr double smoothstep(double t) noexcept {
    const double x = clamp<double>(t, 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}

constexpr double kPi = 3.14159265358979323846;
constexpr double deg_to_rad(double deg) noexcept { return deg * kPi / 180.0; }
constexpr double rad_to_deg(double rad) noexcept { return rad * 180.0 / kPi; }

template <class T>
constexpr int sign(T v) noexcept {
    return (v > T{0}) - (v < T{0});
}

// 浮点比较：布局代码里到处是 " == 0 "，用这个而不是裸比较。
template <class T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
constexpr bool nearly_equal(T a, T b, T eps = T{1e-9}) noexcept {
    const T diff = a > b ? a - b : b - a;
    return diff <= eps;
}

// NaN/Inf 防御：数据来自用户 CSV 时，一个 NaN 会让整张图消失。
template <class T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
constexpr bool is_finite(T v) noexcept {
    return v == v && v != std::numeric_limits<T>::infinity() && v != -std::numeric_limits<T>::infinity();
}

}  // namespace termchart::util

#endif  // TERMCHART_UTIL_MATH_HPP
