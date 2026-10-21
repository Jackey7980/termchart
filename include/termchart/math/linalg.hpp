// math/linalg.hpp —— 稠密小矩阵线性代数（行主序、span、无模板）
//
// 定位：服务 chart3d/chart4d 的 3D/4D 矩阵与数据拟合的最小公倍集——乘法、求逆、
// 解方程、行列式。**刻意不做**通用矩阵类型（那属于 Eigen 的生态位）：这里只认
// span<const double> + 行主序 n×n，零分配、零模板、调用方自管缓冲。
//   * 所有尺寸不匹配/奇异的输入返回 false 且不改 out —— fail-loud。
//   * 算法：部分选主元高斯消元（numerical stability 的底线）。
#ifndef TERMCHART_MATH_LINALG_HPP
#define TERMCHART_MATH_LINALG_HPP

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

}  // namespace termchart::math

#endif  // TERMCHART_MATH_LINALG_HPP
