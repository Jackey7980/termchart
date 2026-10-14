// band.hpp —— 带状比例尺（每个类别占一条等宽带，forward 返回带中心）
//
// 与 CategoryScale 把类别压成"网格点"不同，BandScale 把 [range_min, range_max]
// 均分成 n 条等宽带，第 i 个类别落在第 i 条带的中心。柱状/条形图让柱体落在带
// 内、两侧留 padding 时观感最自然。band_width() 暴露单带宽度供绘制。
#ifndef TERMCHART_SCALE_BAND_HPP
#define TERMCHART_SCALE_BAND_HPP

#include <cstddef>

#include "termchart/export.hpp"
#include "termchart/scale/scale.hpp"
#include "termchart/util/math.hpp"

namespace termchart::scale {

class BandScale final : public Scale {
public:
    BandScale() = default;
    BandScale(std::size_t categories, double range_min = 0.0, double range_max = 1.0,
              double padding = 0.1) noexcept {
        set_categories(categories);
        set_range(range_min, range_max);
        set_padding(padding);
    }

    void set_categories(std::size_t n) noexcept { n_ = n; }
    void set_range(double rmin, double rmax) noexcept {
        rmin_ = rmin;
        rmax_ = rmax;
    }
    void set_padding(double p) noexcept { padding_ = util::clamp(p, 0.0, 0.5); }
    void set_clamp(bool on) noexcept { clamp_ = on; }

    [[nodiscard]] double forward(double data) const noexcept override {
        if (n_ == 0) {
            return rmin_;
        }
        const double band = (rmax_ - rmin_) / static_cast<double>(n_);
        const double inner = band * (1.0 - 2.0 * padding_);
        const double center = rmin_ + (data + 0.5) * band;
        if (clamp_) {
            const double half = inner / 2.0;
            return util::clamp(center, rmin_ + half, rmax_ - half);
        }
        return center;
    }

    [[nodiscard]] double inverse(double pixel) const noexcept override {
        if (n_ == 0) {
            return 0.0;
        }
        const double band = (rmax_ - rmin_) / static_cast<double>(n_);
        double t = (pixel - rmin_) / band - 0.5;
        if (clamp_) {
            t = util::clamp(t, 0.0, static_cast<double>(n_ > 0 ? n_ - 1 : 0));
        }
        return t;
    }

    [[nodiscard]] bool valid() const noexcept override {
        return util::is_finite(rmin_) && util::is_finite(rmax_) && n_ >= 1 &&
               rmax_ > rmin_;
    }

    [[nodiscard]] double domain_min() const noexcept override { return 0.0; }
    [[nodiscard]] double domain_max() const noexcept override {
        return static_cast<double>(n_ > 0 ? n_ - 1 : 0);
    }
    [[nodiscard]] double range_min() const noexcept override { return rmin_; }
    [[nodiscard]] double range_max() const noexcept override { return rmax_; }

    [[nodiscard]] double band_width() const noexcept {
        return n_ > 0 ? (rmax_ - rmin_) / static_cast<double>(n_) : 0.0;
    }
    [[nodiscard]] std::size_t categories() const noexcept { return n_; }
    [[nodiscard]] bool clamped() const noexcept { return clamp_; }

private:
    std::size_t n_ = 0;
    double rmin_ = 0.0;
    double rmax_ = 1.0;
    double padding_ = 0.1;
    bool clamp_ = true;
};

}  // namespace termchart::scale

#endif  // TERMCHART_SCALE_BAND_HPP
