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

#include "termchart/core/point.hpp"  // 2D 版 catmull_rom 的 Point（core 是 L0，math 可依赖）

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

// 单调三次 Hermite（PCHIP，Fritsch–Carlson）：在 CubicSpline 的自然样条之外提供
// 一个**保单调、不过冲**的变体。自然样条二阶导两端为 0，在噪声数据上会轻微过冲、画出
// 数据之外的假峰/假谷；PCHIP 把"相邻割线异号或任一为零"处的切线压成 0，从而守住单调性。
// 构造 O(N)、求值 O(log N)（二分定位区间），接口与 CubicSpline 对齐（valid() / size() / at()）。
// 坏输入（点数 < 2、xs 非严格升序、含 NaN/Inf）不抛异常：valid() 为 false，at() 返回 NaN。
class PchipSpline {
public:
    PchipSpline() noexcept = default;

    PchipSpline(std::span<const double> xs, std::span<const double> ys) noexcept {
        const std::size_t n = xs.size() < ys.size() ? xs.size() : ys.size();
        if (n < 2) return;
        for (std::size_t i = 0; i < n; ++i) {
            if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) return;
            if (i > 0 && !(xs[i] > xs[i - 1])) return;
        }
        x_.assign(xs.begin(), xs.begin() + static_cast<std::ptrdiff_t>(n));
        y_.assign(ys.begin(), ys.begin() + static_cast<std::ptrdiff_t>(n));
        m_.assign(n, 0.0);

        // 区间割线（>0，因为 xs 严格升序）。
        std::vector<double> d(n - 1, 0.0);
        for (std::size_t i = 0; i + 1 < n; ++i) {
            d[i] = (y_[i + 1] - y_[i]) / (x_[i + 1] - x_[i]);
        }

        // 端点切线 = 相邻单侧割线。
        m_[0] = d[0];
        m_[n - 1] = d[n - 2];

        // 内部点：割线加权调和平均（Fritsch–Carlson）。异号或任一为 0 → 切线压 0，守住单调。
        for (std::size_t i = 1; i + 1 < n; ++i) {
            if (d[i - 1] * d[i] <= 0.0) {
                m_[i] = 0.0;
            } else {
                const double w1 = 2.0 * (x_[i + 1] - x_[i]);  // 邻区间长度加权
                const double w2 = x_[i] - x_[i - 1];
                m_[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i]);
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
        const double t = (x - x_[lo]) / h;
        const double t2 = t * t;
        const double t3 = t2 * t;
        // Hermite 基函数（h00/h10/h01/h11）：保证经过端点且切线 m_。
        const double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
        const double h10 = t3 - 2.0 * t2 + t;
        const double h01 = -2.0 * t3 + 3.0 * t2;
        const double h11 = t3 - t2;
        return h00 * y_[lo] + h10 * h * m_[lo] + h01 * y_[hi] + h11 * h * m_[hi];
    }

private:
    std::vector<double> x_;
    std::vector<double> y_;
    std::vector<double> m_;  // 各点切线斜率
    bool valid_ = false;
};

// ── 平滑曲线（折线渲染 / 动画缓动） ──────────────────────────────────────────
//
// 与 CubicSpline（数据域插值、曲线不必过数据点）的定位不同：下面两个是"让已有
// 折线丝滑"的工具——catmull_rom 保证**经过每个途经点**（折线变曲线但不过冲到数据
// 之外），smooth_step 是缓动函数。graph/route/spline 里还有一份走连线路由的
// Catmull-Rom，那是"途径点"语义、互不相关。

// 平滑过渡（GLSL smoothstep 的 3t²-2t³ 缓动）：把 x 从 [edge0, edge1] 归一到 [0,1]
// 再做三次平滑。edge0==edge1（退化区间）返回 0（避免除零）；x 越界钳位到 [0,1]。
// 与 util::smoothstep(double t)（t 已归一化）是同一曲线的两种入参形态，互不冲突。
[[nodiscard]] inline double smooth_step(double edge0, double edge1, double x) noexcept {
    double t = (edge1 == edge0) ? 0.0 : (x - edge0) / (edge1 - edge0);
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return t * t * (3.0 - 2.0 * t);
}

// Catmull-Rom 样条插值（立方，C¹ 连续、过途经点）：给定 4 个控制点 p0..p3，在
// 区间 [p1, p2] 上按参数 t∈[0,1] 求值。t=0 → p1、t=1 → p2；对线性数据精确还原、
// 对弯曲数据自然顺滑。把相邻折线段各取 p0=前一点、p3=后一点，再在 [0,1] 上细分
// 采样，即可得到丝滑曲线。t 越界会外推（可能过冲），调用方自己钳位。
[[nodiscard]] inline double catmull_rom(double p0, double p1, double p2, double p3,
                                        double t) noexcept {
    const double t2 = t * t;
    const double t3 = t2 * t;
    return 0.5 * ((2.0 * p1) + (-p0 + p2) * t +
                  (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
                  (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
}

// 2D 点形：对 x、y 各做一次标量 catmull_rom。
[[nodiscard]] inline Point catmull_rom(const Point& p0, const Point& p1, const Point& p2,
                                       const Point& p3, double t) noexcept {
    return Point{catmull_rom(p0.x, p1.x, p2.x, p3.x, t),
                 catmull_rom(p0.y, p1.y, p2.y, p3.y, t)};
}

// ── 贝塞尔曲线（二次 / 三次） ────────────────────────────────────────────────
//
// 这里是全库贝塞尔求值的唯一真源：TTF 字形轮廓（glyf 表）用**二次**曲线，canvas
// 的 Path（quadratic_to/cubic_to）两者都支持。此前 TTF 光栅化在 flatten_quad 里
// 内联了一遍二次公式，现改为调用本文件——避免"同一公式两份实现"的漂移。
//
// 约定：t 不钳位（与 catmull_rom 一致，调用方负责落在 [0,1]）；t=0 返回起点、
// t=1 返回终点。二次有 1 个控制点，三次有 2 个——**不要混用**：用二次公式套三次
// 控制点会得到错误曲线。

// 二次贝塞尔标量形：B(t) = (1-t)²·p0 + 2(1-t)t·c + t²·p1。
[[nodiscard]] inline double quadratic_bezier(double p0, double c, double p1, double t) noexcept {
    const double mt = 1.0 - t;
    return mt * mt * p0 + 2.0 * mt * t * c + t * t * p1;
}

// 二次贝塞尔点形：对 x、y 各做一次标量求值。
[[nodiscard]] inline Point quadratic_bezier(const Point& p0, const Point& c, const Point& p1,
                                            double t) noexcept {
    return Point{quadratic_bezier(p0.x, c.x, p1.x, t),
                 quadratic_bezier(p0.y, c.y, p1.y, t)};
}

// 三次贝塞尔标量形：B(t) = (1-t)³·p0 + 3(1-t)²t·c1 + 3(1-t)t²·c2 + t³·p1。
[[nodiscard]] inline double cubic_bezier(double p0, double c1, double c2, double p1,
                                         double t) noexcept {
    const double mt = 1.0 - t;
    const double a = mt * mt * mt;
    const double b = 3.0 * mt * mt * t;
    const double c = 3.0 * mt * t * t;
    const double d = t * t * t;
    return a * p0 + b * c1 + c * c2 + d * p1;
}

// 三次贝塞尔点形：对 x、y 各做一次标量求值。
[[nodiscard]] inline Point cubic_bezier(const Point& p0, const Point& c1, const Point& c2,
                                        const Point& p1, double t) noexcept {
    return Point{cubic_bezier(p0.x, c1.x, c2.x, p1.x, t),
                 cubic_bezier(p0.y, c1.y, c2.y, p1.y, t)};
}

}  // namespace termchart::math

#endif  // TERMCHART_MATH_INTERPOLATE_HPP
