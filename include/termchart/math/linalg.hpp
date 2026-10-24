// math/linalg.hpp —— 稠密小矩阵线性代数 + 向量距离/相似度（行主序、span、无模板）
//
// 定位：服务 chart3d/chart4d 的 3D/4D 矩阵与数据拟合的最小公倍集——乘法、求逆、
// 解方程、行列式，以及高维数据用的欧氏距离/余弦相似度。**刻意不做**通用矩阵类型
// （那属于 Eigen 的生态位）：这里只认 span<const double> + 行主序 n×n，零分配、
// 零模板、调用方自管缓冲。
//   * 矩阵运算：尺寸不匹配/奇异的输入返回 false 且不改 out —— fail-loud。
//   * 向量运算：长度不等取较短者；空、含 NaN 或零向量返回 NaN。
//   * 算法：部分选主元高斯消元（numerical stability 的底线）。
#ifndef TERMCHART_MATH_LINALG_HPP
#define TERMCHART_MATH_LINALG_HPP

#include <cmath>
#include <cstddef>
#include <span>

#include "termchart/export.hpp"

namespace termchart::math {

// C = A × B（均为 n×n 行主序）。out 可与 a 或 b 别名（内部用栈缓冲分块）。
[[nodiscard]] TERMCHART_API bool multiply(std::span<const double> a, std::span<const double> b,
                                          std::size_t n, std::span<double> out) noexcept;

// out = A⁻¹（n×n）。A 奇异（主元为 0）时返回 false，out 内容无定义。
[[nodiscard]] TERMCHART_API bool inverse(std::span<const double> a, std::size_t n,
                                         std::span<double> out) noexcept;

// 解 A x = b（A 为 n×n，b/x 长 n）。奇异返回 false。
[[nodiscard]] TERMCHART_API bool solve(std::span<const double> a, std::span<const double> b,
                                       std::size_t n, std::span<double> x) noexcept;

// 行列式（高斯消元上三角求积）。n == 0 返回 1（空矩阵约定的单位元）。
[[nodiscard]] TERMCHART_API double determinant(std::span<const double> a, std::size_t n) noexcept;

// ── 向量距离 / 相似度 ───────────────────────────────────────────────────────
//
// 高维样本（AutoViz 簇间距离、PCA 降维后的近邻、向量检索雏形）只需要两个数：
// 距离和夹角余弦。都按"长度不等取较短者"处理，避免调用方先做长度检查。

// 欧氏距离：√Σ(aᵢ - bᵢ)²。空输入或任一对应元素为 NaN 时返回 NaN。
[[nodiscard]] inline double euclidean_distance(std::span<const double> a,
                                               std::span<const double> b) noexcept {
    const std::size_t n = a.size() < b.size() ? a.size() : b.size();
    if (n == 0) return std::nan("");
    double acc = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double d = a[i] - b[i];
        if (std::isnan(d)) return std::nan("");
        acc += d * d;
    }
    return std::sqrt(acc);
}

// 余弦相似度：a·b / (|a|·|b|) ∈ [-1, 1]。空输入、含 NaN 或任一零向量（无定义）
// 返回 NaN；完全同向为 1，正交为 0，反向为 -1。
[[nodiscard]] inline double cosine_similarity(std::span<const double> a,
                                              std::span<const double> b) noexcept {
    const std::size_t n = a.size() < b.size() ? a.size() : b.size();
    if (n == 0) return std::nan("");
    double dot = 0.0;
    double na = 0.0;
    double nb = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        if (std::isnan(a[i]) || std::isnan(b[i])) return std::nan("");
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    if (!(na > 0.0) || !(nb > 0.0)) return std::nan("");  // 零向量
    return dot / (std::sqrt(na) * std::sqrt(nb));
}

}  // namespace termchart::math

#endif  // TERMCHART_MATH_LINALG_HPP
