// symlog.cpp —— 对称对数比例尺实现
#include "termchart/scale/symlog.hpp"

#include <cmath>

namespace termchart::scale {

SymLogScale::SymLogScale(double domain_min, double domain_max, double range_min,
                         double range_max, double linthresh, bool clamp) noexcept {
    set_domain(domain_min, domain_max);
    set_range(range_min, range_max);
    linthresh_ = linthresh;
    clamp_ = clamp;
}

void SymLogScale::set_domain(double domain_min, double domain_max) noexcept {
    dmin_ = domain_min;
    dmax_ = domain_max;
}

void SymLogScale::set_range(double range_min, double range_max) noexcept {
    rmin_ = range_min;
    rmax_ = range_max;
}

void SymLogScale::set_linthresh(double linthresh) noexcept {
    linthresh_ = linthresh;
}

void SymLogScale::set_clamp(bool on) noexcept { clamp_ = on; }

double SymLogScale::transform(double data) const noexcept {
    if (!util::is_finite(data)) {
        return std::nan("");
    }
    const double c = (linthresh_ > 0.0) ? linthresh_ : 1.0;
    const double ax = std::fabs(data);
    const double t = std::log10(1.0 + ax / c);
    return data < 0.0 ? -t : t;
}

double SymLogScale::forward(double data) const noexcept {
    if (!util::is_finite(data) || !valid()) {
        return std::nan("");
    }
    const double lt0 = transform(dmin_);
    const double lt1 = transform(dmax_);
    const double ldata = transform(data);
    const double lspan = lt1 - lt0;
    double t = (lspan == 0.0) ? 0.0 : (ldata - lt0) / lspan;
    if (clamp_) {
        t = util::clamp(t, 0.0, 1.0);
    }
    return util::lerp(rmin_, rmax_, t);
}

double SymLogScale::inverse(double pixel) const noexcept {
    if (!valid()) {
        return std::nan("");
    }
    const double rspan = rmax_ - rmin_;
    double t = (rspan == 0.0) ? 0.0 : (pixel - rmin_) / rspan;
    if (clamp_) {
        t = util::clamp(t, 0.0, 1.0);
    }
    const double lt0 = transform(dmin_);
    const double lt1 = transform(dmax_);
    const double T = util::lerp(lt0, lt1, t);
    const double c = (linthresh_ > 0.0) ? linthresh_ : 1.0;
    const double mag = c * (std::pow(10.0, std::fabs(T)) - 1.0);
    return T < 0.0 ? -mag : mag;
}

bool SymLogScale::valid() const noexcept {
    return util::is_finite(dmin_) && util::is_finite(dmax_) &&
           util::is_finite(rmin_) && util::is_finite(rmax_) &&
           util::is_finite(linthresh_) && linthresh_ > 0.0 && dmin_ != dmax_;
}

}  // namespace termchart::scale
