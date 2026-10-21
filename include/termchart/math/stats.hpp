// math/stats.hpp —— 描述统计（header-only）
//
// NumPy/pandas 统计函数的最小集：均值、中位数、方差/标准差、协方差、皮尔逊相关。
// 设计约定：
//   * 输入一律 std::span<const double>，不用模板——调用方传 vector/array/C 数组皆可，
//     且每个 TU 只编译一份展开，守住"编译极速"。
//   * NaN 视为缺失并**跳过**（与 util/data/rolling 同口径）；空输入或全 NaN 返回 quiet NaN。
//   * 均值用 Kahan 补偿求和；方差用 Welford 单遍算法——长序列上不丢有效位。
//   * 不抛异常、不分配堆内存（median 内部的局部 vector 除外）。
#ifndef TERMCHART_MATH_STATS_HPP
#define TERMCHART_MATH_STATS_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace termchart::math {

// 算术均值。跳过 NaN；无有效值时返回 NaN。
[[nodiscard]] inline double mean(std::span<const double> xs) noexcept {
    double sum = 0.0;
    double comp = 0.0;  // Kahan 补偿项
    std::size_t n = 0;
    for (const double x : xs) {
        if (std::isnan(x)) continue;
        const double y = x - comp;
        const double t = sum + y;
        comp = (t - sum) - y;
        sum = t;
        ++n;
    }
    if (n == 0) return std::nan("");
    return sum / static_cast<double>(n);
}

// Kahan 补偿求和（避免长序列大数吃小数）；NaN 跳过。
[[nodiscard]] inline double sum(std::span<const double> xs) noexcept {
    double s = 0.0;
    double comp = 0.0;  // Kahan 补偿项
    for (const double x : xs) {
        if (std::isnan(x)) continue;
        const double y = x - comp;
        const double t = s + y;
        comp = (t - s) - y;
        s = t;
    }
    return s;
}

// 中位数。偶数个取中间两个的均值；O(N) 平均复杂度（nth_element）。
[[nodiscard]] inline double median(std::span<const double> xs) noexcept {
    std::vector<double> v;
    v.reserve(xs.size());
    for (const double x : xs) {
        if (!std::isnan(x)) v.push_back(x);
    }
    const std::size_t n = v.size();
    if (n == 0) return std::nan("");
    const std::size_t mid = n / 2;
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid), v.end());
    if (n % 2 == 1) return v[mid];
    const double upper = v[mid];
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid - 1), v.end());
    return 0.5 * (v[mid - 1] + upper);
}

// 方差。ddof = 0 为总体方差（numpy 默认口径），ddof = 1 为样本方差（pandas 默认口径）；
// 有效样本数 ≤ ddof 时返回 NaN。Welford 单遍，数值稳定。
[[nodiscard]] inline double variance(std::span<const double> xs, double ddof = 0.0) noexcept {
    double m = 0.0;
    double m2 = 0.0;
    std::size_t n = 0;
    for (const double x : xs) {
        if (std::isnan(x)) continue;
        ++n;
        const double delta = x - m;
        m += delta / static_cast<double>(n);
        m2 += delta * (x - m);
    }
    if (n == 0 || static_cast<double>(n) <= ddof) return std::nan("");
    return m2 / (static_cast<double>(n) - ddof);
}

// 标准差（方差的平方根），ddof 口径同 variance。
[[nodiscard]] inline double stddev(std::span<const double> xs, double ddof = 0.0) noexcept {
    const double v = variance(xs, ddof);
    if (std::isnan(v)) return v;
    return std::sqrt(v);
}

// 协方差。两序列成对取样：任一侧为 NaN 的对整体跳过；长度不等取较短者。
[[nodiscard]] inline double covariance(std::span<const double> xs, std::span<const double> ys,
                                       double ddof = 0.0) noexcept {
    const std::size_t n = xs.size() < ys.size() ? xs.size() : ys.size();
    double mx = 0.0;
    double my = 0.0;
    double c = 0.0;  // 累积离差积
    std::size_t k = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (std::isnan(xs[i]) || std::isnan(ys[i])) continue;
        ++k;
        const double dx = xs[i] - mx;
        const double dy = ys[i] - my;
        mx += dx / static_cast<double>(k);
        my += dy / static_cast<double>(k);
        c += dx * (ys[i] - my);
    }
    if (k == 0 || static_cast<double>(k) <= ddof) return std::nan("");
    return c / (static_cast<double>(k) - ddof);
}

// 皮尔逊相关系数：cov / (sx * sy)。任一序列零方差（常数）时返回 NaN（无定义）。
[[nodiscard]] inline double pearson(std::span<const double> xs, std::span<const double> ys) noexcept {
    const double sx = stddev(xs);
    const double sy = stddev(ys);
    if (sx == 0.0 || sy == 0.0 || std::isnan(sx) || std::isnan(sy)) return std::nan("");
    return covariance(xs, ys) / (sx * sy);
}

}  // namespace termchart::math

#endif  // TERMCHART_MATH_STATS_HPP
