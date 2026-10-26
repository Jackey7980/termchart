// polar_heat.cpp —— 极坐标热力图实现（极坐标分箱 + 逐格查表上色）
#include "termchart/chart/polar/polar_heat.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "termchart/canvas/primitives.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/detail/block_chars.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/util/math.hpp"

#include "termchart/chart/scene_sink.hpp"

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

// 归一化计数 t → 密度字符（与笛卡尔 heatmap 同一阶梯）：最低档也非空格，
// 否则"有点但很少"的格与"无数据"无法区分。
char32_t heat_char(double t) noexcept {
    constexpr char32_t kLevels[4] = {detail::kLightShade, detail::kMediumShade,
                                     detail::kDarkShade, detail::kFullBlock};
    int idx = static_cast<int>(t * 4.0);
    idx = std::clamp(idx, 0, 3);
    return kLevels[static_cast<std::size_t>(idx)];
}

// 色标：内框上方一条从冷到热的色带 + 名称（沿用笛卡尔 heatmap 的做法）。
void draw_colorbar(canvas::Canvas& canvas, const Rect& inner, color::ColormapKind kind,
                   const std::string& name) {
    const int len = static_cast<int>(std::min(24.0, inner.w));
    if (len < 1) {
        return;
    }
    const double x0 = std::floor(inner.x);
    for (int i = 0; i < len; ++i) {
        const double t = len <= 1 ? 0.0
                                  : static_cast<double>(i) / static_cast<double>(len - 1);
        const Rgb rgb = color::sample(kind, t);
        canvas.put(Point{x0 + static_cast<double>(i), 0.0}, detail::kFullBlock,
                   Style{}.set_fg(Color::rgb(rgb.r, rgb.g, rgb.b)));
    }
    if (!name.empty()) {
        canvas.draw_text(Point{x0 + static_cast<double>(len) + 1.0, 0.0}, name, Style{});
    }
}

}  // namespace

void PolarHeatChart::draw(DrawContext ctx) {
    std::vector<Point> pts;
    pts.reserve(points_.size());
    for (const Point& p : points_) {
        if (std::isfinite(p.x) && std::isfinite(p.y)) {
            pts.push_back(p);
        }
    }
    if (pts.empty()) {
        return;
    }
    double rmax = options_.rmax;
    if (!(rmax > 0.0)) {
        for (const Point& p : pts) {
            rmax = std::max(rmax, p.y);
        }
    }
    if (!(rmax > 0.0)) {
        rmax = 1.0;
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{m.left, m.top,
                     static_cast<double>(ctx.canvas.width()) - m.left - m.right,
                     static_cast<double>(ctx.canvas.height()) - m.top - m.bottom};
    if (inner.w < 3.0 || inner.h < 3.0) {
        return;
    }
    const PolarFrame f = make_frame(inner);
    const double start_rad = util::deg_to_rad(0.0);  // 角度 0 = 东
    const bool clockwise = true;

    const int angle_bins = std::clamp(options_.angle_bins, 1, 360);
    const int radial_bins = std::clamp(options_.radial_bins, 1, 64);
    const auto dim_a = static_cast<std::size_t>(angle_bins);
    const auto dim_r = static_cast<std::size_t>(radial_bins);

    // 分箱计数：角度归一化到 [0,360)，半径钳到满量程；索引一律 clamp。
    std::vector<std::size_t> count(dim_a * dim_r, 0);
    for (const Point& p : pts) {
        double deg = std::fmod(p.x, 360.0);
        if (deg < 0.0) {
            deg += 360.0;
        }
        const double rnorm = std::clamp(p.y / rmax, 0.0, 1.0);
        const auto sec = static_cast<std::size_t>(deg / 360.0 * static_cast<double>(angle_bins));
        const auto ring = static_cast<std::size_t>(rnorm * static_cast<double>(radial_bins));
        const std::size_t s = std::min(sec, dim_a - 1);
        const std::size_t r = std::min(ring, dim_r - 1);
        ++count[r * dim_a + s];
    }
    std::size_t max_count = 0;
    for (std::size_t c : count) {
        max_count = std::max(max_count, c);
    }
    if (max_count == 0) {
        return;  // 理论不可达（pts 非空），保守兜底
    }

    if (ctx.draw_grid) {
        const Style grid_style = Style{}.set_fg(default_color(DefaultColor::Muted));
        const int steps =
            std::max(12, static_cast<int>(std::lround(4.0 * (f.rx + f.ry))));
        for (int i = 0; i < steps; ++i) {
            const double swept =
                kTwoPi * static_cast<double>(i) / static_cast<double>(steps);
            ctx.canvas.put(polar_cell(f, 1.0, swept, start_rad, clockwise), U'\u00B7',
                           grid_style);
        }
    }

    // 逐格上色：查归属箱，空箱留空（保留参考环），非空箱按计数取字符与颜色。
    for (std::uint32_t y = 0; y < ctx.canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < ctx.canvas.width(); ++x) {
            const Point p{static_cast<double>(x), static_cast<double>(y)};
            const double r = norm_radius(f, p);
            if (r > 1.0) {
                continue;
            }
            const double a = chart_angle(f, p, start_rad, clockwise);
            const std::size_t s = std::min(
                static_cast<std::size_t>(a / kTwoPi * static_cast<double>(angle_bins)),
                dim_a - 1);
            const std::size_t rr = std::min(
                static_cast<std::size_t>(r * static_cast<double>(radial_bins)),
                dim_r - 1);
            const std::size_t c = count[rr * dim_a + s];
            if (c == 0) {
                continue;
            }
            const double t = static_cast<double>(c) / static_cast<double>(max_count);
            const Rgb rgb = color::sample(options_.colormap, t);
            ctx.canvas.put(p, heat_char(t),
                           Style{}.set_fg(Color::rgb(rgb.r, rgb.g, rgb.b)));
        }
    }

    if (options_.show_colorbar && ctx.draw_legend) {
        draw_colorbar(ctx.canvas, inner, options_.colormap, options_.name);
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

bool PolarHeatChart::to_scene(scene::Scene& out, std::uint32_t width,
                               std::uint32_t height) const {
    // 与 draw() 同源：剔除非有限点。
    std::vector<Point> pts;
    pts.reserve(points_.size());
    for (const Point& p : points_) {
        if (std::isfinite(p.x) && std::isfinite(p.y)) {
            pts.push_back(p);
        }
    }
    if (pts.empty()) {
        return true;
    }
    double rmax = options_.rmax;
    if (!(rmax > 0.0)) {
        for (const Point& p : pts) {
            rmax = std::max(rmax, p.y);
        }
    }
    if (!(rmax > 0.0)) {
        rmax = 1.0;
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{m.left, m.top,
                     static_cast<double>(width) - m.left - m.right,
                     static_cast<double>(height) - m.top - m.bottom};
    if (inner.w < 3.0 || inner.h < 3.0) {
        return true;
    }
    const PolarFrame f = make_frame(inner);
    const double start_rad = util::deg_to_rad(0.0);
    const bool clockwise = true;

    const int angle_bins = std::clamp(options_.angle_bins, 1, 360);
    const int radial_bins = std::clamp(options_.radial_bins, 1, 64);
    const auto dim_a = static_cast<std::size_t>(angle_bins);
    const auto dim_r = static_cast<std::size_t>(radial_bins);

    // 分箱计数：与 draw() 一致。
    std::vector<std::size_t> count(dim_a * dim_r, 0);
    for (const Point& p : pts) {
        double deg = std::fmod(p.x, 360.0);
        if (deg < 0.0) {
            deg += 360.0;
        }
        const double rnorm = std::clamp(p.y / rmax, 0.0, 1.0);
        const auto sec = static_cast<std::size_t>(deg / 360.0 *
                                                  static_cast<double>(angle_bins));
        const auto ring = static_cast<std::size_t>(rnorm *
                                                   static_cast<double>(radial_bins));
        const std::size_t s = std::min(sec, dim_a - 1);
        const std::size_t r = std::min(ring, dim_r - 1);
        ++count[r * dim_a + s];
    }
    std::size_t max_count = 0;
    for (std::size_t c : count) {
        max_count = std::max(max_count, c);
    }
    if (max_count == 0) {
        return true;
    }

    SceneSink sink(out);
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);

    // 参考环（grid，to_scene 默认开）。
    {
        const Style grid_style = Style{}.set_fg(default_color(DefaultColor::Muted));
        const int steps =
            std::max(12, static_cast<int>(std::lround(4.0 * (f.rx + f.ry))));
        for (int i = 0; i < steps; ++i) {
            const double swept =
                kTwoPi * static_cast<double>(i) / static_cast<double>(steps);
            sink.put(polar_cell(f, 1.0, swept, start_rad, clockwise), U'\u00B7',
                     grid_style);
        }
    }

    // 逐格上色：查归属箱，非空箱按计数取字符与颜色（与 draw() 一致）。
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const Point p{static_cast<double>(x), static_cast<double>(y)};
            const double r = norm_radius(f, p);
            if (r > 1.0) {
                continue;
            }
            const double a = chart_angle(f, p, start_rad, clockwise);
            const std::size_t s = std::min(
                static_cast<std::size_t>(a / kTwoPi * static_cast<double>(angle_bins)),
                dim_a - 1);
            const std::size_t rr = std::min(
                static_cast<std::size_t>(r * static_cast<double>(radial_bins)),
                dim_r - 1);
            const std::size_t c = count[rr * dim_a + s];
            if (c == 0) {
                continue;
            }
            const double t = static_cast<double>(c) / static_cast<double>(max_count);
            const Rgb rgb = color::sample(options_.colormap, t);
            sink.put(p, heat_char(t), Style{}.set_fg(Color::rgb(rgb.r, rgb.g, rgb.b)));
        }
    }

    // 色标（内框上方一条从冷到热的色带 + 名称）。
    if (options_.show_colorbar) {
        const int len = static_cast<int>(std::min(24.0, inner.w));
        if (len >= 1) {
            const double x0 = std::floor(inner.x);
            for (int i = 0; i < len; ++i) {
                const double t = len <= 1
                                     ? 0.0
                                     : static_cast<double>(i) /
                                           static_cast<double>(len - 1);
                const Rgb rgb = color::sample(options_.colormap, t);
                sink.put(Point{x0 + static_cast<double>(i), 0.0}, detail::kFullBlock,
                         Style{}.set_fg(Color::rgb(rgb.r, rgb.g, rgb.b)));
            }
            if (!options_.name.empty()) {
                sink.text(Point{x0 + static_cast<double>(len) + 1.0, 0.0},
                          options_.name, Style{});
            }
        }
    }
    if (!title_.empty()) {
        layout::draw_title(sink, title_, inner);
    }
    b.end_group(0);
    return true;
}

}  // namespace termchart::chart
