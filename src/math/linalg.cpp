#include "termchart/math/linalg.hpp"

#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace termchart::math {
namespace {

// 带部分选主元的高斯消元：把 [A | b...] 原地化为上三角。
// cols 为右侧列数（求逆 = n 列单位阵，解方程 = 1 列）。
// 返回 false 表示矩阵奇异（主元绝对值 < eps）。
bool gauss_eliminate(std::vector<double>& m, std::size_t n, std::size_t cols) noexcept {
    for (std::size_t k = 0; k < n; ++k) {
        // 选主元：第 k 列中 |m[i][k]| 最大者
        std::size_t piv = k;
        double best = std::fabs(m[k * (n + cols) + k]);
        for (std::size_t i = k + 1; i < n; ++i) {
            const double v = std::fabs(m[i * (n + cols) + k]);
            if (v > best) {
                best = v;
                piv = i;
            }
        }
        if (!(best > 1e-300)) return false;  // 奇异（含 NaN 主元：NaN 比较为 false）
        if (piv != k) {
            for (std::size_t c = 0; c < n + cols; ++c) {
                std::swap(m[k * (n + cols) + c], m[piv * (n + cols) + c]);
            }
        }
        const double d = m[k * (n + cols) + k];
        for (std::size_t i = k + 1; i < n; ++i) {
            const double f = m[i * (n + cols) + k] / d;
            if (f == 0.0) continue;
            m[i * (n + cols) + k] = 0.0;
            for (std::size_t c = k + 1; c < n + cols; ++c) {
                m[i * (n + cols) + c] -= f * m[k * (n + cols) + c];
            }
        }
    }
    return true;
}

// 已上三角化系统的回代（右侧 cols 列），结果写回 m 的右侧 cols 列。
void back_substitute(std::vector<double>& m, std::size_t n, std::size_t cols) noexcept {
    for (std::size_t k = n; k-- > 0;) {
        const double d = m[k * (n + cols) + k];
        for (std::size_t c = 0; c < cols; ++c) {
            double v = m[k * (n + cols) + n + c];
            for (std::size_t j = k + 1; j < n; ++j) {
                v -= m[k * (n + cols) + j] * m[j * (n + cols) + n + c];
            }
            m[k * (n + cols) + n + c] = v / d;
        }
    }
}

}  // namespace

bool multiply(std::span<const double> a, std::span<const double> b, std::size_t n,
              std::span<double> out) noexcept {
    if (a.size() < n * n || b.size() < n * n || out.size() < n * n) return false;
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < n; ++k) {
                s += a[i * n + k] * b[k * n + j];
            }
            out[i * n + j] = s;
        }
    }
    return true;
}

bool inverse(std::span<const double> a, std::size_t n, std::span<double> out) noexcept {
    if (a.size() < n * n || out.size() < n * n) return false;
    std::vector<double> m(n * (n + n), 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            m[i * (n + n) + j] = a[i * n + j];
        }
        m[i * (n + n) + n + i] = 1.0;  // 右侧接单位阵
    }
    if (!gauss_eliminate(m, n, n)) return false;
    back_substitute(m, n, n);
    // m 为 n×(n+n) 行主序：右侧 n 列（偏移 n 处）才是逆矩阵。
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            out[i * n + j] = m[i * (n + n) + n + j];
        }
    }
    return true;
}

bool solve(std::span<const double> a, std::span<const double> b, std::size_t n,
           std::span<double> x) noexcept {
    if (a.size() < n * n || b.size() < n || x.size() < n) return false;
    std::vector<double> m(n * (n + 1), 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            m[i * (n + 1) + j] = a[i * n + j];
        }
        m[i * (n + 1) + n] = b[i];
    }
    if (!gauss_eliminate(m, n, 1)) return false;
    back_substitute(m, n, 1);
    for (std::size_t i = 0; i < n; ++i) {
        x[i] = m[i * (n + 1) + n];
    }
    return true;
}

double determinant(std::span<const double> a, std::size_t n) noexcept {
    if (a.size() < n * n) return std::nan("");
    if (n == 0) return 1.0;
    std::vector<double> m(n * n, 0.0);
    for (std::size_t i = 0; i < n * n; ++i) {
        m[i] = a[i];
    }
    double det = 1.0;
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t piv = k;
        double best = std::fabs(m[k * n + k]);
        for (std::size_t i = k + 1; i < n; ++i) {
            const double v = std::fabs(m[i * n + k]);
            if (v > best) {
                best = v;
                piv = i;
            }
        }
        if (!(best > 1e-300)) return 0.0;  // 奇异
        if (piv != k) {
            for (std::size_t c = 0; c < n; ++c) {
                std::swap(m[k * n + c], m[piv * n + c]);
            }
            det = -det;  // 行交换改号
        }
        det *= m[k * n + k];
        const double d = m[k * n + k];
        for (std::size_t i = k + 1; i < n; ++i) {
            const double f = m[i * n + k] / d;
            if (f == 0.0) continue;
            for (std::size_t c = k; c < n; ++c) {
                m[i * n + c] -= f * m[k * n + c];
            }
        }
    }
    return det;
}

}  // namespace termchart::math
