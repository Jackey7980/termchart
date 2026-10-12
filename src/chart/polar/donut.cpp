// donut.cpp —— 环形图实现（饼图填充 + 内孔判定，环带内的格才落笔）
#include "termchart/chart/polar/donut.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

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

constexpr std::array<char32_t, 9> kSectorChars{{
    U'\u2588',  // █
    U'\u2593',  // ▓
    U'\u2592',  // ▒
    U'\u2591',  // ░
    U'\u25A0',  // ■
    U'\u25CF',  // ●
    U'\u25B2',  // ▲
    U'\u25C6',  // ◆
    U'\u25CB',  // ○
}};

}  // namespace

void DonutChart::set_values(std::vector<double> values, std::vector<std::string> labels) {
    values_ = std::move(values);
    labels_ = std::move(labels);
}

void DonutChart::draw(DrawContext ctx) {
    std::vector<double> vals;
    std::vector<std::string> names;
    double total = 0.0;
    for (std::size_t i = 0; i < values_.size(); ++i) {
        const double v = values_[i];
        if (!(v > 0.0) || !std::isfinite(v)) {
            continue;
        }
        vals.push_back(v);
        names.push_back(i < labels_.size() ? labels_[i] : std::string{});
        total += v;
    }
    if (vals.empty() || !(total > 0.0)) {
        return;
    }

    const double hole = std::clamp(options_.hole_ratio, 0.0, 0.9);
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{m.left, m.top,
                     static_cast<double>(ctx.canvas.width()) - m.left - m.right,
                     static_cast<double>(ctx.canvas.height()) - m.top - m.bottom};
    if (inner.w < 3.0 || inner.h < 3.0) {
        return;
    }
    const PolarFrame f = make_frame(inner);
    const double start_rad = util::deg_to_rad(options_.start_angle);

    // 环带填充：hole ≤ rnorm ≤ 1 才落笔；内孔留空（中心格必为空）。
    for (std::uint32_t y = 0; y < ctx.canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < ctx.canvas.width(); ++x) {
            const Point p{static_cast<double>(x), static_cast<double>(y)};
            const double r = norm_radius(f, p);
            if (r > 1.0 || r < hole) {
                continue;
            }
            const double a = chart_angle(f, p, start_rad, options_.clockwise);
            std::size_t idx = vals.size() - 1;
            double acc = 0.0;
            for (std::size_t i = 0; i < vals.size(); ++i) {
                acc += kTwoPi * vals[i] / total;
                if (a < acc) {
                    idx = i;
                    break;
                }
            }
            const Rgb rgb = color::series_color(color::PaletteKind::Default, idx);
            ctx.canvas.put(p, kSectorChars[idx % kSectorChars.size()],
                           Style{}.set_fg(Color::rgb(rgb.r, rgb.g, rgb.b)));
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
