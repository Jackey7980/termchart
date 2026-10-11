#include "termchart/scale/auto_domain.hpp"

#include <algorithm>
#include <cmath>

#include "termchart/scale/nice.hpp"
#include "termchart/util/math.hpp"

namespace termchart::scale {

Domain nice_domain(double data_min, double data_max, bool include_zero) noexcept {
    Domain domain;
    if (!util::is_finite(data_min) || !util::is_finite(data_max)) {
        return domain;
    }

    double lo = data_min;
    double hi = data_max;

    if (lo == hi) {
        // 退化：给一个以该值为中心、宽 1 的对称域，避免除零与零宽轴。
        const double center = lo;
        lo = center - 0.5;
        hi = center + 0.5;
    } else {
        const double pad = (hi - lo) * 0.05;  // 5% 留白
        lo -= pad;
        hi += pad;
    }

    if (include_zero) {
        lo = std::min(lo, 0.0);
        hi = std::max(hi, 0.0);
    }

    const double step = nice_step(hi - lo, 5);
    if (!(step > 0.0)) {
        domain.min = lo;
        domain.max = hi;
        domain.ok = true;
        return domain;
    }
    // 把两端吸附到 nice 步长的整数倍。
    domain.min = std::floor(lo / step) * step;
    domain.max = std::ceil(hi / step) * step;
    domain.ok = true;
    return domain;
}

}  // namespace termchart::scale
