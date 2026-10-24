#include "termchart/color/gradient.hpp"

#include <algorithm>

#include "termchart/math/color_space.hpp"
#include "termchart/util/math.hpp"

namespace termchart::color {

Gradient::Gradient(Rgb from, Rgb to) noexcept {
    add_stop(0.0, from);
    add_stop(1.0, to);
}

Gradient& Gradient::set_mode(GradientMode mode) noexcept {
    mode_ = mode;
    return *this;
}

Gradient& Gradient::add_stop(double pos, Rgb color) {
    const double clamped = util::clamp<double>(pos, 0.0, 1.0);
    const auto it = std::find_if(stops_.begin(), stops_.end(),
                                 [clamped](const Stop& s) { return s.pos == clamped; });
    if (it != stops_.end()) {
        it->color = color;
        return *this;
    }
    stops_.push_back(Stop{clamped, color});
    std::sort(stops_.begin(), stops_.end(),
              [](const Stop& a, const Stop& b) { return a.pos < b.pos; });
    return *this;
}

Gradient& Gradient::clear_stops() noexcept {
    stops_.clear();
    return *this;
}

Rgb Gradient::sample(double t) const noexcept {
    if (stops_.empty()) {
        return Rgb{};
    }
    const double x = util::clamp<double>(t, 0.0, 1.0);
    if (stops_.size() == 1 || x <= stops_.front().pos) {
        return stops_.front().color;
    }
    if (x >= stops_.back().pos) {
        return stops_.back().color;
    }
    const std::size_t upper_index =
        static_cast<std::size_t>(std::upper_bound(stops_.begin(), stops_.end(), x,
                                                  [](double v, const Stop& s) { return v < s.pos; }) -
                                 stops_.begin());
    const Stop& lo = stops_[upper_index - 1];
    const Stop& hi = stops_[upper_index];
    const double span = hi.pos - lo.pos;
    if (span <= 0.0) {
        return hi.color;
    }
    return mix(lo.color, hi.color, (x - lo.pos) / span, mode_);
}

std::vector<Rgb> Gradient::generate(std::size_t count) const {
    std::vector<Rgb> out;
    if (count == 0) {
        return out;
    }
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const double t = count == 1 ? 0.0
                                    : static_cast<double>(i) / static_cast<double>(count - 1);
        out.push_back(sample(t));
    }
    return out;
}

bool Gradient::empty() const noexcept { return stops_.empty(); }

std::size_t Gradient::stop_count() const noexcept { return stops_.size(); }

Rgb mix(Rgb from, Rgb to, double t) noexcept {
    const double x = util::clamp<double>(t, 0.0, 1.0);
    return Rgb{static_cast<std::uint8_t>(std::lround(util::lerp<double>(
                   static_cast<double>(from.r), static_cast<double>(to.r), x))),
               static_cast<std::uint8_t>(std::lround(util::lerp<double>(
                   static_cast<double>(from.g), static_cast<double>(to.g), x))),
               static_cast<std::uint8_t>(std::lround(util::lerp<double>(
                   static_cast<double>(from.b), static_cast<double>(to.b), x)))};
}

Rgb mix(Rgb from, Rgb to, double t, GradientMode mode) noexcept {
    if (mode == GradientMode::Rgb) {
        return mix(from, to, t);
    }
    const double x = util::clamp<double>(t, 0.0, 1.0);
    double h1 = 0.0;
    double s1 = 0.0;
    double v1 = 0.0;
    double h2 = 0.0;
    double s2 = 0.0;
    double v2 = 0.0;
    math::rgb_to_hsv(from, h1, s1, v1);
    math::rgb_to_hsv(to, h2, s2, v2);

    // 灰（s = 0）没有色相：两端皆灰就没有色环可走，退回分量插值；
    // 只有一端是灰时借另一端的色相，否则"灰 → 蓝"会从红色起步。
    if (s1 <= 0.0 && s2 <= 0.0) {
        return mix(from, to, x);
    }
    if (s1 <= 0.0) {
        h1 = h2;
    } else if (s2 <= 0.0) {
        h2 = h1;
    }

    // 走环上的最短弧：色相是角度，直接线性插值会让"红 → 品红"绕 300°。
    double dh = h2 - h1;
    if (dh > 180.0) {
        dh -= 360.0;
    } else if (dh < -180.0) {
        dh += 360.0;
    }
    double hue = h1 + dh * x;
    if (hue < 0.0) {
        hue += 360.0;
    } else if (hue >= 360.0) {
        hue -= 360.0;
    }
    return math::hsv_to_rgb(hue, util::lerp<double>(s1, s2, x), util::lerp<double>(v1, v2, x));
}

Rgb contrasting_text_color(Rgb background) noexcept {
    return background.luma() > 128.0 ? Rgb{16, 16, 16} : Rgb{245, 245, 245};
}

}  // namespace termchart::color
