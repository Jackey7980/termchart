// rose.cpp —— 南丁格尔玫瑰实现（等角扇区 + 半径编码数值的格粒度填充）
#include "termchart/chart/polar/rose.hpp"

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

// 沿角度采样画圆环：步长约每周 4·(rx+ry) 格，保证相邻采样点格间连续
// （与 canvas::arc_points 同法；格粒度下必有台阶，不做假精度）。
void draw_ring(canvas::Canvas& canvas, const PolarFrame& f, char32_t ch, const Style& st) {
    const int steps = std::max(12, static_cast<int>(std::lround(4.0 * (f.rx + f.ry))));
    for (int i = 0; i < steps; ++i) {
        const double ma = kTwoPi * static_cast<double>(i) / static_cast<double>(steps);
        const Point p{f.center.x + f.rx * std::cos(ma),
                      f.center.y - f.ry * std::sin(ma)};
        canvas.put(p, ch, st);
    }
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

void RoseChart::set_values(std::vector<double> values, std::vector<std::string> labels) {
    values_ = std::move(values);
    labels_ = std::move(labels);
}

void RoseChart::draw(DrawContext ctx) {
    std::vector<double> vals;
    std::vector<std::string> names;
    for (std::size_t i = 0; i < values_.size(); ++i) {
        const double v = values_[i];
        if (!(v > 0.0) || !std::isfinite(v)) {
            continue;
        }
        vals.push_back(v);
        names.push_back(i < labels_.size() ? labels_[i] : std::string{});
    }
    if (vals.empty()) {
        return;
    }
    double vmax = 0.0;
    for (double v : vals) {
        vmax = std::max(vmax, v);
    }
    if (!(vmax > 0.0)) {
        return;  // 理论不可达（已滤非正值），保守兜底
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

    if (ctx.draw_grid) {
        // 外圈参考环 = 最大半径刻度。
        draw_ring(ctx.canvas, f, U'\u00B7',
                  Style{}.set_fg(default_color(DefaultColor::Muted)));
    }

    // 逐格填充：扇区等角，落笔判据 rnorm ≤ value/vmax。
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
            if (r > vals[idx] / vmax) {
                continue;  // 超出该扇区的数值半径
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
