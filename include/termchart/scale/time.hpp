// time.hpp —— 时间比例尺（时间戳数值线性映射，语义化时间轴）
//
// 把时间（以数值表示的时间戳，例如 Unix 秒/毫秒；单位由调用方统一）线性映射到
// 显示域。calendar-aware 的刻度（年/月/日/时）由 ticks 模块按数据范围选取，
// 本类只负责坐标变换。与 LinearScale 同构，单列一类以明确"这是时间轴"语义，
// 便于图表层识别并切换到时间刻度策略。
#ifndef TERMCHART_SCALE_TIME_HPP
#define TERMCHART_SCALE_TIME_HPP

#include "termchart/export.hpp"
#include "termchart/scale/scale.hpp"
#include "termchart/util/math.hpp"

namespace termchart::scale {

class TimeScale final : public Scale {
public:
    TimeScale() = default;
    TimeScale(double domain_min, double domain_max, double range_min = 0.0,
              double range_max = 1.0, bool clamp = true) noexcept {
        set_domain(domain_min, domain_max);
        set_range(range_min, range_max);
        clamp_ = clamp;
    }

    void set_domain(double dmin, double dmax) noexcept {
        dmin_ = dmin;
        dmax_ = dmax;
    }
    void set_range(double rmin, double rmax) noexcept {
        rmin_ = rmin;
        rmax_ = rmax;
    }
    void set_clamp(bool on) noexcept { clamp_ = on; }

    [[nodiscard]] double forward(double data) const noexcept override {
        const double span = dmax_ - dmin_;
        double t = span == 0.0 ? 0.0 : (data - dmin_) / span;
        if (clamp_) {
            t = util::clamp(t, 0.0, 1.0);
        }
        return util::lerp(rmin_, rmax_, t);
    }

    [[nodiscard]] double inverse(double pixel) const noexcept override {
        const double rspan = rmax_ - rmin_;
        double t = rspan == 0.0 ? 0.0 : (pixel - rmin_) / rspan;
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

#endif  // TERMCHART_SCALE_TIME_HPP
