// category.hpp —— 类别比例尺（离散类别等距排布）
//
// 把 0..n-1 的类别索引线性映射到 [range_min, range_max]：第 i 个类别落在
// 等距网格点上。柱形/条形/分类型坐标轴常用。forward(类别索引) 取对应网格点，
// inverse(像素) 反算最近类别索引（用于命中判定）。
#ifndef TERMCHART_SCALE_CATEGORY_HPP
#define TERMCHART_SCALE_CATEGORY_HPP

#include <cstddef>

#include "termchart/export.hpp"
#include "termchart/scale/scale.hpp"
#include "termchart/util/math.hpp"

namespace termchart::scale {

class CategoryScale final : public Scale {
public:
    CategoryScale() = default;
    explicit CategoryScale(std::size_t categories, double range_min = 0.0,
                          double range_max = 1.0) noexcept {
        set_categories(categories);
        set_range(range_min, range_max);
    }

    void set_categories(std::size_t n) noexcept { n_ = n; }
    void set_range(double rmin, double rmax) noexcept {
        rmin_ = rmin;
        rmax_ = rmax;
    }
    void set_clamp(bool on) noexcept { clamp_ = on; }

    [[nodiscard]] double forward(double data) const noexcept override {
        if (n_ <= 1) {
            return rmin_;
        }
        double t = data / static_cast<double>(n_ - 1);
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
        return t * static_cast<double>(n_ > 1 ? n_ - 1 : 0);
    }

    [[nodiscard]] bool valid() const noexcept override {
        return util::is_finite(rmin_) && util::is_finite(rmax_) && n_ >= 1;
    }

    [[nodiscard]] double domain_min() const noexcept override { return 0.0; }
    [[nodiscard]] double domain_max() const noexcept override {
        return static_cast<double>(n_ > 0 ? n_ - 1 : 0);
    }
    [[nodiscard]] double range_min() const noexcept override { return rmin_; }
    [[nodiscard]] double range_max() const noexcept override { return rmax_; }

    [[nodiscard]] std::size_t categories() const noexcept { return n_; }
    [[nodiscard]] bool clamped() const noexcept { return clamp_; }

private:
    std::size_t n_ = 0;
    double rmin_ = 0.0;
    double rmax_ = 1.0;
    bool clamp_ = true;
};

}  // namespace termchart::scale

#endif  // TERMCHART_SCALE_CATEGORY_HPP
