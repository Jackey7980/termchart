#include "termchart/scale/nice.hpp"

#include <algorithm>

namespace termchart::scale {

double nice_number(double x, bool round) noexcept {
    if (!(x > 0.0)) {
        return 0.0;  // 非正输入无意义；调用方应已过滤
    }
    const double exp = std::floor(std::log10(x));
    const double base = std::pow(10.0, exp);
    const double frac = x / base;  // 1 <= frac < 10

    double nice;
    if (round) {
        if (frac < 1.5) {
            nice = 1.0;
        } else if (frac < 3.0) {
            nice = 2.0;
        } else if (frac < 7.0) {
            nice = 5.0;
        } else {
            nice = 10.0;
        }
    } else {
        if (frac <= 1.0) {
            nice = 1.0;
        } else if (frac <= 2.0) {
            nice = 2.0;
        } else if (frac <= 5.0) {
            nice = 5.0;
        } else {
            nice = 10.0;
        }
    }
    return nice * base;
}

double nice_step(double span, int target_ticks) noexcept {
    if (!(span > 0.0) || target_ticks <= 0) {
        return 0.0;
    }
    const double rough = span / static_cast<double>(target_ticks);
    return nice_number(rough, true);
}

}  // namespace termchart::scale
