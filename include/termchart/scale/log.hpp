// log.hpp —— 对数比例尺
//
// 域必须为正（或同时为负，这里只支持正域——负域先取绝对值再映射，调用方处理）。
// 零与负数在 forward 里返回 NaN：对数域里它们没有位置，UI 应在此之前过滤或
// 切到线性子段，而不是让整张图消失。
#ifndef TERMCHART_SCALE_LOG_HPP
#define TERMCHART_SCALE_LOG_HPP

#include <cmath>

#include "termchart/export.hpp"
#include "termchart/scale/scale.hpp"
#include "termchart/util/math.hpp"

namespace termchart::scale {

class LogScale final : public Scale {
public:
    LogScale() = default;
    LogScale(double domain_min, double domain_max, double range_min = 0.0,
             double range_max = 1.0, double base = 10.0, bool clamp = true) noexcept {
        set_domain(domain_min, domain_max);
        set_range(range_min, range_max);
        base_ = base;
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
    void set_base(double base) noexcept { base_ = base; }
    void set_clamp(bool on) noexcept { clamp_ = on; }

    [[nodiscard]] double forward(double data) const noexcept override {
        if (!util::is_finite(data) || data <= 0.0 || !valid()) {
            return std::nan("");
        }
        const double lmin = std::log(dmin_) / std::log(base_);
        const double lmax = std::log(dmax_) / std::log(base_);
        const double ldata = std::log(data) / std::log(base_);
        const double lspan = lmax - lmin;
        double t = (lspan == 0.0) ? 0.0 : (ldata - lmin) / lspan;
        if (clamp_) {
            t = util::clamp(t, 0.0, 1.0);
        }
        return util::lerp(rmin_, rmax_, t);
    }

    [[nodiscard]] double inverse(double pixel) const noexcept override {
        if (!valid()) {
            return std::nan("");
        }
        const double rspan = rmax_ - rmin_;
        double t = (rspan == 0.0) ? 0.0 : (pixel - rmin_) / rspan;
        if (clamp_) {
            t = util::clamp(t, 0.0, 1.0);
        }
        const double lmin = std::log(dmin_) / std::log(base_);
        const double lmax = std::log(dmax_) / std::log(base_);
        return std::pow(base_, util::lerp(lmin, lmax, t));
    }

    [[nodiscard]] bool valid() const noexcept override {
        return util::is_finite(dmin_) && util::is_finite(dmax_) &&
               util::is_finite(base_) && base_ > 0.0 && base_ != 1.0 &&
               dmin_ > 0.0 && dmax_ > 0.0 && dmin_ != dmax_;
    }

    [[nodiscard]] double domain_min() const noexcept override { return dmin_; }
    [[nodiscard]] double domain_max() const noexcept override { return dmax_; }
    [[nodiscard]] double range_min() const noexcept override { return rmin_; }
    [[nodiscard]] double range_max() const noexcept override { return rmax_; }
    [[nodiscard]] double base() const noexcept { return base_; }

private:
    double dmin_ = 1.0;
    double dmax_ = 10.0;
    double rmin_ = 0.0;
    double rmax_ = 1.0;
    double base_ = 10.0;
    bool clamp_ = true;
};

}  // namespace termchart::scale

#endif  // TERMCHART_SCALE_LOG_HPP
