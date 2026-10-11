#include "termchart/scale/ticks.hpp"

#include <algorithm>
#include <cmath>

#include "termchart/scale/nice.hpp"
#include "termchart/util/math.hpp"

namespace termchart::scale {

std::vector<double> nice_ticks(double domain_min, double domain_max, int target) noexcept {
    std::vector<double> out;
    if (!util::is_finite(domain_min) || !util::is_finite(domain_max) || target <= 0) {
        return out;
    }
    if (domain_min > domain_max) {
        std::swap(domain_min, domain_max);
    }
    if (domain_min == domain_max) {
        out.push_back(domain_min);
        return out;
    }

    const double step = nice_step(domain_max - domain_min, target);
    if (!(step > 0.0)) {
        return out;
    }
    // 第一个不小于 domain_min 的整步；+1e-9 容差避免浮点把边界刻度漏掉。
    const double first = std::ceil(domain_min / step - 1e-9) * step;
    for (double v = first; v <= domain_max + step * 1e-9; v += step) {
        out.push_back(v);
    }
    return out;
}

std::vector<double> ticks_for(const Scale& scale, int target) noexcept {
    std::vector<double> out;
    if (!scale.valid() || target <= 0) {
        return out;
    }
    const double rmin = scale.range_min();
    const double rmax = scale.range_max();
    const double span = rmax - rmin;
    if (span == 0.0) {
        return out;
    }

    const double dmin = scale.domain_min();
    const double dmax = scale.domain_max();
    const double step = nice_step(dmax - dmin, target);
    if (!(step > 0.0)) {
        return out;
    }
    const double first = std::ceil(dmin / step - 1e-9) * step;
    for (double v = first; v <= dmax + step * 1e-9; v += step) {
        const double pixel = scale.forward(v);
        if (pixel >= rmin - 1e-9 && pixel <= rmax + 1e-9) {
            out.push_back(v);
        }
    }
    return out;
}

}  // namespace termchart::scale
