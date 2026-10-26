// easing.cpp —— 缓动曲线实现（自 app/animation.cpp 下沉，行为逐字不变）
#include "termchart/util/easing.hpp"

#include "termchart/util/math.hpp"

namespace termchart::util {

double ease(Easing easing, double t) noexcept {
    if (!is_finite(t)) {
        return 0.0;  // NaN/Inf 不可比较，视为界外。
    }
    const double x = clamp<double>(t, 0.0, 1.0);
    switch (easing) {
        case Easing::Linear:
            return x;
        case Easing::EaseInQuad:
            return x * x;
        case Easing::EaseOutQuad:
            return 1.0 - (1.0 - x) * (1.0 - x);
        case Easing::EaseInOutQuad:
            return x < 0.5 ? 2.0 * x * x : 1.0 - 2.0 * (1.0 - x) * (1.0 - x);
        case Easing::EaseInCubic:
            return x * x * x;
        case Easing::EaseOutCubic:
            return 1.0 - (1.0 - x) * (1.0 - x) * (1.0 - x);
        case Easing::EaseInOutCubic:
            return x < 0.5 ? 4.0 * x * x * x
                           : 1.0 - 4.0 * (1.0 - x) * (1.0 - x) * (1.0 - x);
    }
    return x;
}

}  // namespace termchart::util
