// linear.hpp —— 线性比例尺（最常用，也是其他比例尺的退化形态）
//
// 默认构造即「单位映射」domain [0,1] → range [0,1]，方便在未知数据范围时
// 先占位、后 set_domain。clamp 默认开启：绘图 API 的越界输入极其常见，
// 默认密封比默认崩溃友好。
#ifndef TERMCHART_SCALE_LINEAR_HPP
#define TERMCHART_SCALE_LINEAR_HPP

#include "termchart/export.hpp"
#include "termchart/scale/scale.hpp"
#include "termchart/util/math.hpp"

namespace termchart::scale {

class LinearScale final : public Scale {
public:
    LinearScale() = default;
    LinearScale(double domain_min, double domain_max, double range_min = 0.0,
                double range_max = 1.0, bool clamp = true) noexcept {
        set_domain(domain_min, domain_max);
        set_range(range_min, range_max);
        clamp_ = clamp;
    }

    void set_domain(double domain_min, double domain_max) noexcept {
        dmin_ = domain_min;
        dmax_ = domain_max;
    }
    void set_range(double range_min, double range_max) noexcept {
        rmin_ = range_min;
        rmax_ = range_max;
    }
    void set_clamp(bool on) noexcept { clamp_ = on; }

    [[nodiscard]] double forward(double data) const noexcept override {
        const double span = dmax_ - dmin_;
        double t = (span == 0.0) ? 0.0 : (data - dmin_) / span;
        if (clamp_) {
            t = util::clamp(t, 0.0, 1.0);
        }
        return util::lerp(rmin_, rmax_, t);
    }

    [[nodiscard]] double inverse(double pixel) const noexcept override {
        const double rspan = rmax_ - rmin_;
        double t = (rspan == 0.0) ? 0.0 : (pixel - rmin_) / rspan;
        if (clamp_) {
            t = util::clamp(t, 0.0, 1.0);
        }
        return util::lerp(dmin_, dmax_, t);
    }

    [[nodiscard]] bool valid() const noexcept override {
        return util::is_finite(dmin_) && util::is_finite(dmax_) &&
               util::is_finite(rmin_) && util::is_finite(rmax_) && dmin_ != dmax_;
    }

    [[nodiscard]] double domain_min() const noexcept override { return dmin_; }
    [[nodiscard]] double domain_max() const noexcept override { return dmax_; }
    [[nodiscard]] double range_min() const noexcept override { return rmin_; }
    [[nodiscard]] double range_max() const noexcept override { return rmax_; }
    [[nodiscard]] bool clamped() const noexcept { return clamp_; }

private:
    double dmin_ = 0.0;
    double dmax_ = 1.0;
    double rmin_ = 0.0;
    double rmax_ = 1.0;
    bool clamp_ = true;
};

}  // namespace termchart::scale

#endif  // TERMCHART_SCALE_LINEAR_HPP
