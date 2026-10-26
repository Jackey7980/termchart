// symlog.hpp —— 对称对数比例尺（SymLog）
//
// 普通对数轴无法表达负数与零（它们在 log 域没有位置，只能丢弃整段数据）。
// SymLog 在零附近保留一段「线性区」（阈值 C = linthresh），线性区之外再走对数，
// 因此能在同一张轴上同时容纳大跨度的正负数据，且平滑穿过 0。变换：
//
//     T(x) = sign(x) * log10(1 + |x| / C)
//
// 当 C → 0 时退化为纯对数；当 C → ∞ 时退化为纯线性。C 越大，零附近线性区越宽。
#ifndef TERMCHART_SCALE_SYMLOG_HPP
#define TERMCHART_SCALE_SYMLOG_HPP

#include "termchart/export.hpp"
#include "termchart/scale/scale.hpp"
#include "termchart/util/math.hpp"

namespace termchart::scale {

class SymLogScale final : public Scale {
public:
    SymLogScale() = default;
    SymLogScale(double domain_min, double domain_max, double range_min = 0.0,
                double range_max = 1.0, double linthresh = 1.0,
                bool clamp = true) noexcept;

    void set_domain(double domain_min, double domain_max) noexcept;
    void set_range(double range_min, double range_max) noexcept;
    void set_linthresh(double linthresh) noexcept;
    void set_clamp(bool on) noexcept;

    // T(x) = sign(x) * log10(1 + |x| / C) —— 对称对数核心映射（不夹紧）。
    [[nodiscard]] double transform(double data) const noexcept;

    [[nodiscard]] double forward(double data) const noexcept override;
    [[nodiscard]] double inverse(double pixel) const noexcept override;

    [[nodiscard]] bool valid() const noexcept override;

    [[nodiscard]] double domain_min() const noexcept override { return dmin_; }
    [[nodiscard]] double domain_max() const noexcept override { return dmax_; }
    [[nodiscard]] double range_min() const noexcept override { return rmin_; }
    [[nodiscard]] double range_max() const noexcept override { return rmax_; }
    [[nodiscard]] double linthresh() const noexcept { return linthresh_; }
    [[nodiscard]] bool clamped() const noexcept { return clamp_; }

private:
    double dmin_ = -10.0;
    double dmax_ = 10.0;
    double rmin_ = 0.0;
    double rmax_ = 1.0;
    double linthresh_ = 1.0;
    bool clamp_ = true;
};

}  // namespace termchart::scale

#endif  // TERMCHART_SCALE_SYMLOG_HPP
