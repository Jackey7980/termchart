// radar.cpp —— 雷达图实现（蛛网与数据多边形都走 DDA 线段离散）
#include "termchart/chart/polar/radar.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "termchart/canvas/primitives.hpp"
#include "termchart/layout/legend.hpp"
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

// 图表角（第 i 根轴的扫角）→ 格坐标：画布 y 向下为正，故 sin 取负。
Point polar_cell(const PolarFrame& f, double rnorm, double swept, double start_rad,
                 bool clockwise) noexcept {
    const double ma = clockwise ? (start_rad - swept) : (start_rad + swept);
    return Point{f.center.x + rnorm * f.rx * std::cos(ma),
                 f.center.y - rnorm * f.ry * std::sin(ma)};
}

void draw_segment(canvas::Canvas& canvas, Point from, Point to, char32_t ch,
                  const Style& st) {
    for (const Point& p : canvas::line_points(from, to)) {
        canvas.put(p, ch, st);
    }
}

}  // namespace

void RadarChart::set_values(std::vector<double> values, std::vector<std::string> labels) {
    values_ = std::move(values);
    labels_ = std::move(labels);
}

void RadarChart::draw(DrawContext ctx) {
    const std::size_t n = values_.size();
    if (n < 3) {
        return;  // 少于 3 根轴不成多边形
    }
    double vmax = 0.0;
    for (double v : values_) {
        if (std::isfinite(v)) {
            vmax = std::max(vmax, v);
        }
    }
    if (!(vmax > 0.0)) {
        vmax = 1.0;  // 全零数据：蛛网照画，数据多边形缩在中心
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
    const double slot = kTwoPi / static_cast<double>(n);

    const Style web_style = Style{}.set_fg(default_color(DefaultColor::Muted));
    const Style data_style = Style{}.set_fg(color_);

    // 蛛网：3 层环（1/3、2/3、1 半径处的多边形）+ 每根轴的射线。
    if (ctx.draw_grid) {
        for (const double ring : {1.0 / 3.0, 2.0 / 3.0, 1.0}) {
            for (std::size_t i = 0; i < n; ++i) {
                const double swept = slot * static_cast<double>(i);
                const Point from = polar_cell(f, ring, swept, start_rad, options_.clockwise);
                const Point to =
                    polar_cell(f, ring, swept + slot, start_rad, options_.clockwise);
                draw_segment(ctx.canvas, from, to, U'\u00B7', web_style);  // ·
            }
        }
        for (std::size_t i = 0; i < n; ++i) {
            const Point tip = polar_cell(f, 1.0, slot * static_cast<double>(i),
                                         start_rad, options_.clockwise);
            draw_segment(ctx.canvas, f.center, tip, U'\u00B7', web_style);
        }
    }

    // 数据多边形：'•' 描边 + '●' 顶点。
    std::vector<Point> data_cells;
    data_cells.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double v = std::isfinite(values_[i]) ? values_[i] : 0.0;
        const double rnorm = std::clamp(v / vmax, 0.0, 1.0);
        data_cells.push_back(polar_cell(f, rnorm, slot * static_cast<double>(i),
                                        start_rad, options_.clockwise));
    }
    for (std::size_t i = 0; i < n; ++i) {
        draw_segment(ctx.canvas, data_cells[i], data_cells[(i + 1) % n], U'\u2022',
                     data_style);
    }
    for (const Point& p : data_cells) {
        ctx.canvas.put(p, U'\u25CF', data_style);
    }

    // 轴标签：外顶点外推 1.15 半径，越界由 canvas 裁剪。
    for (std::size_t i = 0; i < n && i < labels_.size(); ++i) {
        if (labels_[i].empty()) {
            continue;
        }
        const Point at = polar_cell(f, 1.15, slot * static_cast<double>(i), start_rad,
                                    options_.clockwise);
        ctx.canvas.draw_text(at, labels_[i], Style{});
    }

    if (options_.show_legend && ctx.draw_legend) {
        const std::vector<layout::LegendEntry> entries{
            layout::LegendEntry{name_, color_}};
        const Rect legend_rect{inner.x, 0.0, inner.w, 1.0};
        layout::draw_legend(ctx.canvas, entries, legend_rect, layout::HAlign::Left);
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

bool RadarChart::to_scene(scene::Scene& out, std::uint32_t width,
                           std::uint32_t height) const {
    const std::size_t n = values_.size();
    if (n < 3) {
        return true;  // 少于 3 根轴不成多边形
    }
    double vmax = 0.0;
    for (double v : values_) {
        if (std::isfinite(v)) {
            vmax = std::max(vmax, v);
        }
    }
    if (!(vmax > 0.0)) {
        vmax = 1.0;  // 全零数据：蛛网照画，数据多边形缩在中心
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{m.left, m.top,
                     static_cast<double>(width) - m.left - m.right,
                     static_cast<double>(height) - m.top - m.bottom};
    if (inner.w < 3.0 || inner.h < 3.0) {
        return true;
    }
    const PolarFrame f = make_frame(inner);
    const double start_rad = util::deg_to_rad(options_.start_angle);
    const double slot = kTwoPi / static_cast<double>(n);

    const Style web_style = Style{}.set_fg(default_color(DefaultColor::Muted));
    const Style data_style = Style{}.set_fg(color_);

    SceneSink sink(out);
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);

    // 蛛网：3 层环（多边形）+ 每根轴的射线（与 draw() 一致）。
    for (const double ring : {1.0 / 3.0, 2.0 / 3.0, 1.0}) {
        for (std::size_t i = 0; i < n; ++i) {
            const double swept = slot * static_cast<double>(i);
            const Point from = polar_cell(f, ring, swept, start_rad, options_.clockwise);
            const Point to =
                polar_cell(f, ring, swept + slot, start_rad, options_.clockwise);
            for (const Point& p : canvas::line_points(from, to)) {
                sink.put(p, U'\u00B7', web_style);
            }
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        const Point tip = polar_cell(f, 1.0, slot * static_cast<double>(i),
                                     start_rad, options_.clockwise);
        for (const Point& p : canvas::line_points(f.center, tip)) {
            sink.put(p, U'\u00B7', web_style);
        }
    }

    // 数据多边形：'•' 描边 + '●' 顶点。
    std::vector<Point> data_cells;
    data_cells.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double v = std::isfinite(values_[i]) ? values_[i] : 0.0;
        const double rnorm = std::clamp(v / vmax, 0.0, 1.0);
        data_cells.push_back(polar_cell(f, rnorm, slot * static_cast<double>(i),
                                        start_rad, options_.clockwise));
    }
    for (std::size_t i = 0; i < n; ++i) {
        for (const Point& p :
             canvas::line_points(data_cells[i], data_cells[(i + 1) % n])) {
            sink.put(p, U'\u2022', data_style);
        }
    }
    for (const Point& p : data_cells) {
        sink.put(p, U'\u25CF', data_style);
    }

    // 轴标签：外顶点外推 1.15 半径。
    for (std::size_t i = 0; i < n && i < labels_.size(); ++i) {
        if (labels_[i].empty()) {
            continue;
        }
        const Point at = polar_cell(f, 1.15, slot * static_cast<double>(i), start_rad,
                                    options_.clockwise);
        sink.text(at, labels_[i], Style{});
    }

    if (options_.show_legend) {
        const std::vector<layout::LegendEntry> entries{
            layout::LegendEntry{name_, color_}};
        const Rect legend_rect{inner.x, 0.0, inner.w, 1.0};
        layout::draw_legend(sink, entries, legend_rect, layout::HAlign::Left);
    }
    if (!title_.empty()) {
        layout::draw_title(sink, title_, inner);
    }
    b.end_group(0);
    return true;
}

}  // namespace termchart::chart
