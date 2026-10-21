// math/interpolate.hpp —— 数据插值（header-only）
//
// 让生硬的折线瞬间丝滑：线性插值（零参数、保单调段）与自然三次样条（C² 连续）。
// 设计约定：
//   * 输入 std::span<const double>，xs 必须**严格升序**（图表 x 轴的常态）。
//   * 越界**钳位**到首/末值（不外推：外推出的值没有数据支撑，画成图会骗人）。
//   * NaN 传播：查询点或两端任一为 NaN 时返回 NaN。
//   * 自然样条的二阶导在两端为 0；数据噪声大时可能轻微过冲，monotone (PCHIP)
//     变体记入 backlog，刻意不塞进本文件。
//   * 与 graph/route/spline 的区别：那边是连线路由的 Catmull-Rom（途经点），
//     这边是数据域的插值，互不相干。
#ifndef TERMCHART_MATH_INTERPOLATE_HPP
#define TERMCHART_MATH_INTERPOLATE_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace termchart::math {

// 分段线性插值：在 xs 中定位 x 所在区间，按比例取 ys。
// xs 为空 / x 为 NaN / 定位到的两端任一为 NaN → NaN。
[[nodiscard]] inline double linear_at(std::span<const double> xs, std::span<const double> ys,
                                      double x) noexcept {
    if (xs.empty() || std::isnan(x)) return std::nan("");
    const std::size_t n = xs.size() < ys.size() ? xs.size() : ys.size();
    if (n == 0) return std::nan("");
    if (x <= xs[0]) return ys[0];
    if (x >= xs[n - 1]) return ys[n - 1];
    const auto it = std::lower_bound(xs.begin(), xs.begin() + static_cast<std::ptrdiff_t>(n), x);
    if (it == xs.begin()) return ys[0];
    const std::size_t hi = static_cast<std::size_t>(it - xs.begin());
    const std::size_t lo = hi - 1;
    const double span = xs[hi] - xs[lo];
    if (span <= 0.0 || std::isnan(span)) return ys[lo];
    const double t = (x - xs[lo]) / span;
    return ys[lo] + (ys[hi] - ys[lo]) * t;
}

// 自然三次样条（C² 连续，两端二阶导为 0）。
//
// 构造为 O(N)（Thomas 算法解三对角），求值 O(log N)（二分定位区间）。
// 坏输入（点数 < 2、xs 非严格升序、含 NaN/Inf）不抛异常：valid() 为 false，
// at() 一律返回 NaN —— fail-loud 由调用方检查 valid() 完成。
class CubicSpline {
public:
    CubicSpline() noexcept = default;

    // 构造：xs/ys 等长（取较短者），xs 严格升序。
    CubicSpline(std::span<const double> xs, std::span<const double> ys) noexcept {
        const std::size_t n = xs.size() < ys.size() ? xs.size() : ys.size();
        if (n < 2) return;
        for (std::size_t i = 0; i < n; ++i) {
            if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) return;
            if (i > 0 && !(xs[i] > xs[i - 1])) return;
        }
        x_.assign(xs.begin(), xs.begin() + static_cast<std::ptrdiff_t>(n));
        y_.assign(ys.begin(), ys.begin() + static_cast<std::ptrdiff_t>(n));
        m2_.assign(n, 0.0);

        // 三对角系统：h[i]*M[i-1] + 2(h[i-1]+h[i])*M[i] + h[i-1]*M[i+1] = 6*Δ²y[i]
        // 自然边界 M[0] = M[n-1] = 0，只解内部 n-2 个未知量。
        const std::size_t inner = n - 2;
        std::vector<double> lower(inner, 0.0);  // 次对角
        std::vector<double> diag(inner, 0.0);   // 主对角
        std::vector<double> upper(inner, 0.0);  // 超对角
        std::vector<double> rhs(inner, 0.0);
        for (std::size_t i = 1; i <= n - 2; ++i) {
            const std::size_t k = i - 1;
            const double h_prev = x_[i] - x_[i - 1];
            const double h_next = x_[i + 1] - x_[i];
            lower[k] = h_prev;
            diag[k] = 2.0 * (h_prev + h_next);
            upper[k] = h_next;
            rhs[k] = 6.0 * ((y_[i + 1] - y_[i]) / h_next - (y_[i] - y_[i - 1]) / h_prev);
        }
        // Thomas：前向消去
        for (std::size_t k = 1; k < inner; ++k) {
            const double w = lower[k] / diag[k - 1];
            diag[k] -= w * upper[k - 1];
            rhs[k] -= w * rhs[k - 1];
        }
        // 回代
        if (inner > 0) {
            m2_[n - 2] = rhs[inner - 1] / diag[inner - 1];
            for (std::size_t k = inner - 1; k-- > 0;) {
                m2_[k + 1] = (rhs[k] - upper[k] * m2_[k + 2]) / diag[k];
            }
        }
        valid_ = true;
    }

    [[nodiscard]] bool valid() const noexcept { return valid_; }
    [[nodiscard]] std::size_t size() const noexcept { return x_.size(); }

    // 求值：x 越界钳位到首/末；无效样条或 NaN 查询返回 NaN。
    [[nodiscard]] double at(double x) const noexcept {
        if (!valid_ || std::isnan(x)) return std::nan("");
        const std::size_t n = x_.size();
        if (x <= x_[0]) return y_[0];
        if (x >= x_[n - 1]) return y_[n - 1];
        const auto it = std::lower_bound(x_.begin(), x_.end(), x);
        const std::size_t hi = static_cast<std::size_t>(it - x_.begin());
        const std::size_t lo = hi - 1;
        const double h = x_[hi] - x_[lo];
        const double a = (x_[hi] - x) / h;
        const double b = (x - x_[lo]) / h;
        // S(x) = a*y[i] + b*y[i+1] + ((a³-a)*M[i] + (b³-b)*M[i+1]) * h²/6
        return a * y_[lo] + b * y_[hi] +
               ((a * a * a - a) * m2_[lo] + (b * b * b - b) * m2_[hi]) * (h * h) / 6.0;
    }

private:
    std::vector<double> x_;
    std::vector<double> y_;
    std::vector<double> m2_;  // 各点二阶导
    bool valid_ = false;
};

}  // namespace termchart::math

#endif  // TERMCHART_MATH_INTERPOLATE_HPP
