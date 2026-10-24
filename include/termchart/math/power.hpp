// power.hpp —— 幂与开立方（header-only，属 cmake/header_only.txt）
//
// 为什么不直接用 std::pow / std::cbrt：
//   * 两者在 C++20 都不是 constexpr，色相/亮度换算就进不了常量求值；
//   * std::pow(x, 1.0/3.0) 的精度天生不如专门的立方根迭代。
//
// 刻意不提供 safe_pow：在色彩里把"定义域外"静默折成 0，等于把错误伪装成纯黑，
// 比 NaN 更难查。需要兜底的调用点自己写清策略，这里只做数学上正确的那部分。
#ifndef TERMCHART_MATH_POWER_HPP
#define TERMCHART_MATH_POWER_HPP

#include <bit>
#include <cstdint>
#include <limits>

namespace termchart::math {

// 整数次幂：二进制快速幂。exp < 0 返回 1/base^(-exp)；按 IEEE，0^0 = 1、0^-1 = inf。
//
// 坑：直接写 `-exp`，当 exp == INT_MIN 时是有符号溢出 UB（-INT_MIN 不可表示）。
// 这里先加宽到 long long 再取反，INT_MIN 也能安全变成 2147483648。
constexpr double int_pow(double base, int exp) noexcept {
    const long long e = exp;
    const bool neg = e < 0;
    const unsigned long long n = neg ? static_cast<unsigned long long>(-e)
                                     : static_cast<unsigned long long>(e);
    double acc = 1.0;
    double cur = base;
    unsigned long long rest = n;
    while (rest != 0) {
        if ((rest & 1ULL) != 0) {
            acc *= cur;
        }
        rest >>= 1;
        cur *= cur;
    }
    return neg ? 1.0 / acc : acc;
}

// 立方根：0 / ±inf / NaN 原样返回，其余走"位技巧初值 + Newton"。
//
// Newton 迭代 g ← (2g + a/g²)/3 是二次收敛的，但初值给 1.0 时对 1e-300 这种量级
// 每步只降到 2/3，要几百步才够——所以初值必须按指数域缩放。把 double 的指数位
// 除以 3 再配一个偏移魔数，能得到 ~5% 的相对误差，之后 6 步就到 double 全精度。
constexpr double cbrt(double x) noexcept {
    const double ax = x < 0.0 ? -x : x;  // NaN 走 else 分支，仍是 NaN
    if (!(ax > 0.0) || ax == std::numeric_limits<double>::infinity()) {
        return x;  // 0 / NaN / inf
    }
    std::uint64_t bits = std::bit_cast<std::uint64_t>(ax);
    bits = bits / 3ULL + 0x2A9F7893C3A1C0F0ULL;
    double g = std::bit_cast<double>(bits);
    for (int i = 0; i < 6; ++i) {
        g = (2.0 * g + ax / (g * g)) / 3.0;
    }
    return x < 0.0 ? -g : g;
}

}  // namespace termchart::math

#endif  // TERMCHART_MATH_POWER_HPP
