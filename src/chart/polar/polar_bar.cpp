// polar_bar.cpp —— 极坐标柱状图实现（角域槽位 × 半径域的格粒度填充）
#include "termchart/chart/polar/polar_bar.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "termchart/canvas/primitives.hpp"
#include "termchart/color/palette.hpp"
#include "termchart/layout/legend.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/util/math.hpp"

namespace termchart::chart {
namespace {

constexpr double kTwoPi = 2.0 * util::kPi;

struct PolarFrame {
    Point center;
    double rx = 0.0;
    double ry = 0.0;
};

PolarFrame make_frame(const Rect& inner) noexcept {
    return PolarFrame{inner.center(), inner.w / 2.0, inner.h / 2.0};
}

double norm_radius(const PolarFrame& f, Point p) noexcept {
    const double dx = (p.x - f.center.x) / f.rx;
    const double dy = (p.y - f.center.y) / f.ry;
    return std::sqrt(dx * dx + dy * dy);
}

double chart_angle(const PolarFrame& f, Point p, double start_rad, bool clockwise) noexcept {
    const double ma = std::atan2(f.center.y - p.y, p.x - f.center.x);
    double a = clockwise ? (start_rad - ma) : (ma - start_rad);
    a = std::fmod(a, kTwoPi);
    return a < 0.0 ? a + kTwoPi : a;
}

Point polar_cell(const PolarFrame& f, double rnorm, double swept, double start_rad,
                 bool clockwise) noexcept {
    const double ma = clockwise ? (start_rad - swept) : (start_rad + swept);
    return Point{f.center.x + rnorm * f.rx * std::cos(ma),
                 f.center.y - rnorm * f.ry * std::sin(ma)};
}

// 参考环/射线：沿角度采样（与 canvas::arc_points 同法，格粒度下必有台阶）。
void draw_ring(canvas::Canvas& canvas, const PolarFrame& f, double rnorm,
               double start_rad, bool clockwise, char32_t ch, const Style& st) {
    const int steps = std::max(12, static_cast<int>(std::lround(4.0 * (f.rx + f.ry))));
    for (int i = 0; i < steps; ++i) {
        const double swept = kTwoPi * static_cast<double>(i) / static_cast<double>(steps);
        canvas.put(polar_cell(f, rnorm, swept, start_rad, clockwise), ch, st);
    }
}

void draw_spoke(canvas::Canvas& canvas, const PolarFrame& f, double swept,
                double start_rad, bool clockwise, char32_t ch, const Style& st) {
    const Point tip = polar_cell(f, 1.0, swept, start_rad, clockwise);
    for (const Point& p : canvas::line_points(f.center, tip)) {
        canvas.put(p, ch, st);
    }
}

}  // namespace

void PolarBarChart::set_values(std::vector<double> values,
                               std::vector<std::string> categories) {
    values_ = std::move(values);
    categories_ = std::move(categories);
}

void PolarBarChart::draw(DrawContext ctx) {
    std::vector<double> vals;
    std::vector<std::string> names;
    for (std::size_t i = 0; i < values_.size(); ++i) {
        const double v = values_[i];
        if (!(v > 0.0) || !std::isfinite(v)) {
            continue;
        }
        vals.push_back(v);
        names.push_back(i < categories_.size() ? categories_[i] : std::string{});
    }
    if (vals.empty()) {
        return;
    }
    double vmax = 0.0;
    for (double v : vals) {
        vmax = std::max(vmax, v);
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{m.left, m.top,
                     static_cast<double>(ctx.canvas.width()) - m.left - m.right,
                     static_cast<double>(ctx.canvas.height()) - m.top - m.bottom};
    if (inner.w < 3.0 || inner.h < 3.0) {
        return;
    }
    const PolarFrame f = make_frame(inner);
    const double start_rad = util::deg_to_rad(options_.start_angle);
    const double slot = kTwoPi / static_cast<double>(vals.size());
    const double bar = std::clamp(options_.bar_width, 0.05, 1.0);
    const double pad = slot * (1.0 - bar) / 2.0;  // 槽位内两侧留白

    if (ctx.draw_grid) {
        const Style grid_style = Style{}.set_fg(default_color(DefaultColor::Muted));
        for (const double ring : {0.25, 0.5, 0.75, 1.0}) {
            draw_ring(ctx.canvas, f, ring, start_rad, options_.clockwise, U'\u00B7',
                      grid_style);
        }
        for (std::size_t i = 0; i < vals.size(); ++i) {
            draw_spoke(ctx.canvas, f, slot * static_cast<double>(i), start_rad,
                       options_.clockwise, U'\u00B7', grid_style);
        }
    }

    // 柱体：格的归属角落在柱的角域、且 rnorm ≤ value/vmax，写 '█'。
    for (std::uint32_t y = 0; y < ctx.canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < ctx.canvas.width(); ++x) {
            const Point p{static_cast<double>(x), static_cast<double>(y)};
            const double r = norm_radius(f, p);
            if (r > 1.0) {
                continue;
            }
            const double a = chart_angle(f, p, start_rad, options_.clockwise);
            std::size_t idx = vals.size() - 1;
            const auto k = static_cast<std::size_t>(a / slot);
            idx = std::min(k, vals.size() - 1);
            const double local = a - slot * static_cast<double>(idx);
            if (local < pad || local > slot - pad) {
                continue;  // 槽位留白：属于柱间的空隙
            }
            if (r > vals[idx] / vmax) {
                continue;  // 超出柱高
            }
            const Rgb rgb = color::series_color(color::PaletteKind::Default, idx);
            ctx.canvas.put(p, U'\u2588', Style{}.set_fg(Color::rgb(rgb.r, rgb.g, rgb.b)));
        }
    }

    if (options_.show_legend && ctx.draw_legend) {
        std::vector<layout::LegendEntry> entries;
        entries.reserve(names.size());
        for (std::size_t i = 0; i < names.size(); ++i) {
            const Rgb rgb = color::series_color(color::PaletteKind::Default, i);
            entries.push_back(
                layout::LegendEntry{names[i], Color::rgb(rgb.r, rgb.g, rgb.b)});
        }
        const Rect legend_rect{inner.x, 0.0, inner.w, 1.0};
        layout::draw_legend(ctx.canvas, entries, legend_rect, layout::HAlign::Left);
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

}  // namespace termchart::chart
