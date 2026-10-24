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

// 协方差矩阵（d×d，对称，行主序）。data 为 n×d 行主序样本；out 至少 d*d。
// ddof = 0 为总体协方差（与上面的 covariance 默认一致），ddof = 1 为样本协方差
// （pandas.cov / 多数 PCA 教程的默认口径）。
// 逐行 listwise 删除：某行任一维为 NaN/Inf 则整行跳过（与 PcaChart 的清洗口径一致）。
// 尺寸不匹配、d == 0，或有效行数 ≤ ddof 时返回 false 且不改 out。
[[nodiscard]] inline bool covariance_matrix(std::span<const double> data, std::size_t n,
                                            std::size_t d, std::span<double> out,
                                            double ddof = 0.0) noexcept {
    if (d == 0 || n == 0) return false;
    if (data.size() < n * d || out.size() < d * d) return false;

    // listwise 删除：只保留每一维都有限的整行
    std::vector<std::size_t> rows;
    rows.reserve(n);
    for (std::size_t r = 0; r < n; ++r) {
        bool ok = true;
        for (std::size_t j = 0; j < d; ++j) {
            if (!std::isfinite(data[r * d + j])) {
                ok = false;
                break;
            }
        }
        if (ok) rows.push_back(r);
    }
    const std::size_t k = rows.size();
    if (k == 0 || static_cast<double>(k) <= ddof) return false;

    // 列均值（对有效行）
    std::vector<double> mean(d, 0.0);
    for (const std::size_t r : rows) {
        for (std::size_t j = 0; j < d; ++j) {
            mean[j] += data[r * d + j];
        }
    }
    for (std::size_t j = 0; j < d; ++j) {
        mean[j] /= static_cast<double>(k);
    }

    // 上三角计算后镜像——协方差矩阵天然对称，省一半乘法。
    const double denom = static_cast<double>(k) - ddof;
    for (std::size_t i = 0; i < d; ++i) {
        for (std::size_t j = i; j < d; ++j) {
            double acc = 0.0;
            for (const std::size_t r : rows) {
                acc += (data[r * d + i] - mean[i]) * (data[r * d + j] - mean[j]);
            }
            acc /= denom;
            out[i * d + j] = acc;
            out[j * d + i] = acc;
        }
    }
    return true;
}

// 皮尔逊相关系数：cov / (sx * sy)。任一序列零方差（常数）时返回 NaN（无定义）。
[[nodiscard]] inline double pearson(std::span<const double> xs, std::span<const double> ys) noexcept {
    const double sx = stddev(xs);
    const double sy = stddev(ys);
    if (sx == 0.0 || sy == 0.0 || std::isnan(sx) || std::isnan(sy)) return std::nan("");
    return covariance(xs, ys) / (sx * sy);
}

// ── 分位数与极值篱 ───────────────────────────────────────────────────────────
//
// 这三个是"极值裁剪"的地基：AutoViz 的稳健 Y 轴域（Tukey 篱）要它，chart 的
// KDE 带宽（Silverman 规则）也要它。此前仓库里分散着两份实现（data::quartiles
// 走 sort + 线性插值、chart/statistical/distribution.cpp 里一份匿名 quantile），
// 口径相同却各写一遍——"同一个数字两个真源"正是分叉的起点。这里给出唯一真源：
// **线性插值法**（numpy.percentile / pandas.quantile 的默认口径），
// 即 pos = p·(n-1)，按 floor/ceil 两点插值。
//
// 与 math::median 严格一致：quantile(xs, 0.5) == median(xs)（含偶数样本）。

namespace detail_stats {

// 分位数（v 允许乱序，会被部分排序；v 中不得含 NaN）。
[[nodiscard]] inline double quantile_of(std::vector<double>& v, double p) noexcept {
    const std::size_t n = v.size();
    if (n == 0) return std::nan("");
    if (std::isnan(p)) return std::nan("");
    if (p <= 0.0) return *std::min_element(v.begin(), v.end());
    if (p >= 1.0) return *std::max_element(v.begin(), v.end());
    if (n == 1) return v[0];

    const double pos = p * static_cast<double>(n - 1);
    const auto hi = static_cast<std::size_t>(std::ceil(pos));
    const auto lo = static_cast<std::size_t>(std::floor(pos));
    // 先把 hi 位就位，再在 [0, hi] 前缀里把 lo 位就位：两次 nth_element 都只
    // 触碰必要区间，均摊 O(N)，比整列排序省一个 log。
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(hi), v.end());
    const double b = v[hi];
    if (lo == hi) return b;
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(lo),
                     v.begin() + static_cast<std::ptrdiff_t>(hi) + 1);
    const double a = v[lo];
    const double t = pos - static_cast<double>(lo);
    return a * (1.0 - t) + b * t;
}

}  // namespace detail_stats

// 原地部分排序后返回第 k 小（0 基 k）的元素。
//
// 委托 std::nth_element——它是 introselect（期望 O(N)，最坏 O(N) 由堆排序兜底），
// 手写 quickselect 在三数取中、大量重复键、递归深度上都更容易出错，且不会更快。
// 这里给出的价值是**契约**：k ≥ size 或空输入返回 NaN（不越界、不 UB）；
// NaN 元素会破坏偏序，调用方须先滤掉（quantile 已代劳）。
[[nodiscard]] inline double quickselect(std::span<double> xs, std::size_t k) noexcept {
    if (k >= xs.size()) return std::nan("");
    std::nth_element(xs.begin(), xs.begin() + static_cast<std::ptrdiff_t>(k), xs.end());
    return xs[k];
}

// 分位数 p ∈ [0,1]（越界钳位；p 为 NaN 返回 NaN）。线性插值法，见文件头口径说明。
// NaN 元素视为缺失并跳过；有效值 0 个返回 NaN。拷贝一次（O(N) 空间），不排序整列。
[[nodiscard]] inline double quantile(std::span<const double> xs, double p) noexcept {
    std::vector<double> v;
    v.reserve(xs.size());
    for (const double x : xs) {
        if (!std::isnan(x)) v.push_back(x);
    }
    return detail_stats::quantile_of(v, p);
}

// Tukey 篱（箱线图的"须"）：[Q1 - k·IQR, Q3 + k·IQR]。k = 1.5 是箱线图默认
// （判"离群"），k = 3.0 判"极端离群"。
//
// IQR 退化（过半取值相同，典型："999 个 1 加 1 个 1e21"）时篱失去尺度，改用
// median ± k·max(|median|, 1e-9)：绝对下限 1e-9 挡住 median = 0 时篱宽收缩成 0、
// 把所有非零值都判成离群的退化。这条"统计量塌缩时怎么办"是数学决定，因此放在
// 这里，而不是让每个调用方各写一遍。
struct TukeyFence {
    double lo = 0.0;      // 下篱
    double hi = 0.0;      // 上篱
    double q1 = 0.0;
    double median = 0.0;
    double q3 = 0.0;
    double iqr = 0.0;
    bool valid = false;   // 至少含 1 个有效值；false 时其余字段无意义

    [[nodiscard]] bool contains(double v) const noexcept {
        return valid && v >= lo && v <= hi;
    }
};

// NaN 跳过。单点样本（n == 1）也返回 valid：q1 = q3 = median = 该值，篱靠退化
// 分支撑开——"一个值谈不上分布"，但调用方仍需一个能画的域。
[[nodiscard]] inline TukeyFence tukey_fence(std::span<const double> xs, double k = 1.5) noexcept {
    TukeyFence f;
    std::vector<double> v;
    v.reserve(xs.size());
    for (const double x : xs) {
        if (!std::isnan(x)) v.push_back(x);
    }
    if (v.empty()) return f;

    f.q1 = detail_stats::quantile_of(v, 0.25);
    f.median = detail_stats::quantile_of(v, 0.50);
    f.q3 = detail_stats::quantile_of(v, 0.75);
    f.iqr = f.q3 - f.q1;
    f.valid = true;
    if (f.iqr > 0.0 && k > 0.0) {
        f.lo = f.q1 - k * f.iqr;
        f.hi = f.q3 + k * f.iqr;
        return f;
    }
    // 篱退化：改用相对中位数的尺度（k ≤ 0 视作"不要篱"，但仍给出一个非零宽度的域）。
    const double spread = std::max(std::abs(f.median), 1e-9);
    const double kk = k > 0.0 ? k : 1.5;
    f.lo = f.median - kk * spread;
    f.hi = f.median + kk * spread;
    return f;
}

}  // namespace termchart::math

#endif  // TERMCHART_MATH_STATS_HPP
