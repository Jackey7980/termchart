// math/special.hpp —— 特殊函数（header-only）
//
// Gamma / lnGamma / erf / erfc：对 C++11 <cmath> 数学特殊函数的薄封装——统一 NaN
// 边界口径（阶乘在 0 与负整数无定义，标准库回 ±Inf 的场合这里显式 NaN），
// 行为跨平台确定（MinGW/libstdc++/libc++ 的 tgamma 边界实现不一）。
// Bessel J 系：标准库没有，自行实现——小/中参数幂级数 + 大参数 Hankel 渐近，
// 分界 |x| = 20（两侧均留一位数余量）。
#ifndef TERMCHART_MATH_SPECIAL_HPP
#define TERMCHART_MATH_SPECIAL_HPP

#include <cmath>
#include <cstddef>

namespace termchart::math {

// Gamma 函数 Γ(x)。x 为 0 或负整数（无定义）及 NaN/Inf 时返回 NaN。
[[nodiscard]] inline double gamma(double x) noexcept {
    if (std::isnan(x) || std::isinf(x)) return std::nan("");
    const double r = std::floor(x);
    if (r == x && r <= 0.0) return std::nan("");  // 极点
    return std::tgamma(x);
}

// ln Γ(x)。x ≤ 0 或 NaN/Inf 返回 NaN。大参数下用 log 避免溢出。
[[nodiscard]] inline double ln_gamma(double x) noexcept {
    if (std::isnan(x) || std::isinf(x) || x <= 0.0) return std::nan("");
    return std::lgamma(x);
}

// 误差函数 erf(x)（标准库薄封装，保留 NaN 传播）。
[[nodiscard]] inline double erf(double x) noexcept { return std::erf(x); }

// 互补误差函数 erfc(x) = 1 - erf(x)（大 x 下比手算 1-erf 更保精度）。
[[nodiscard]] inline double erfc(double x) noexcept { return std::erfc(x); }

namespace detail {

// Hankel 渐近的 P 系数（μ = 4ν²）。
inline double bessel_asym_p(double mu, double x) noexcept {
    const double y = 8.0 * x;
    double p = 1.0;
    double term = 1.0;
    // P = 1 - (μ-1)(μ-9)/(2!(8x)²) + (μ-1)(μ-9)(μ-25)(μ-49)/(4!(8x)⁴) - …
    double f = mu - 1.0;
    term *= -f / (2.0 * y * y);
    p += term;
    f *= (mu - 9.0) * (mu - 25.0) * (mu - 49.0);
    term *= -f / (24.0 * y * y * y * y);
    p += term;
    return p;
}

// Hankel 渐近的 Q 系数（μ = 4ν²）。
inline double bessel_asym_q(double mu, double x) noexcept {
    const double y = 8.0 * x;
    double q = (mu - 1.0) / y;
    // Q = (μ-1)/(8x) - (μ-1)(μ-9)(μ-25)/(3!(8x)³) + …
    q -= (mu - 1.0) * (mu - 9.0) * (mu - 25.0) / (6.0 * y * y * y);
    return q;
}

inline double bessel_asym_j(int order, double x) noexcept {
    const double mu = 4.0 * static_cast<double>(order) * static_cast<double>(order);
    const double w = x - static_cast<double>(order) * 1.5707963267948966 - 0.7853981633974483;
    return std::sqrt(2.0 / (3.14159265358979323846 * x)) *
           (std::cos(w) * bessel_asym_p(mu, x) - std::sin(w) * bessel_asym_q(mu, x));
}

}  // namespace detail

// 第一类 Bessel 函数 J0(x)。负参数按偶函数处理。
[[nodiscard]] inline double bessel_j0(double x) noexcept {
    if (std::isnan(x)) return std::nan("");
    const double ax = std::fabs(x);
    if (ax <= 20.0) {
        // J0 = Σ (-1)^k (x²/4)^k / (k!)²
        double term = 1.0;
        double sum = 1.0;
        const double q = ax * ax / 4.0;
        for (int k = 1; k < 80; ++k) {
            term *= -q / (static_cast<double>(k) * static_cast<double>(k));
            sum += term;
            if (std::fabs(term) < 1e-17 * (std::fabs(sum) + 1e-300)) break;
        }
        return sum;
    }
    return detail::bessel_asym_j(0, ax);
}

// 第一类 Bessel 函数 J1(x)。负参数按奇函数处理（J1(-x) = -J1(x)）。
[[nodiscard]] inline double bessel_j1(double x) noexcept {
    if (std::isnan(x)) return std::nan("");
    const double ax = std::fabs(x);
    const double sign = x < 0.0 ? -1.0 : 1.0;
    if (ax <= 20.0) {
        // J1 = (x/2) Σ (-1)^k (x²/4)^k / (k!(k+1)!)
        double term = ax / 2.0;
        double sum = term;
        const double q = ax * ax / 4.0;
        for (int k = 1; k < 80; ++k) {
            term *= -q / (static_cast<double>(k) * static_cast<double>(k + 1));
            sum += term;
            if (std::fabs(term) < 1e-17 * (std::fabs(sum) + 1e-300)) break;
        }
        return sign * sum;
    }
    return sign * detail::bessel_asym_j(1, ax);
}

// 第一类 Bessel 函数 Jn(x)（n ≥ 2 用向上递推 J_{k+1} = (2k/x)J_k - J_{k-1}）。
// 递推在 x > n 时数值稳定；x ≤ n 时仍可用但精度递减（文档如实标注）。
// n < 0 按 J_{-n} = (-1)^n J_n 归算；x == 0 时 n == 0 → 1，否则 → 0。
[[nodiscard]] inline double bessel_j(int n, double x) noexcept {
    if (std::isnan(x)) return std::nan("");
    if (n < 0) {
        const double v = bessel_j(-n, x);
        return (n % 2 == 0) ? v : -v;
    }
    if (x == 0.0) return n == 0 ? 1.0 : 0.0;
    if (n == 0) return bessel_j0(x);
    if (n == 1) return bessel_j1(x);
    double jm1 = bessel_j0(x);  // J_{k-1}
    double jk = bessel_j1(x);   // J_k
    for (int k = 1; k < n; ++k) {
        const double jp = (2.0 * static_cast<double>(k) / x) * jk - jm1;
        jm1 = jk;
        jk = jp;
    }
    return jk;
}

}  // namespace termchart::math

#endif  // TERMCHART_MATH_SPECIAL_HPP
