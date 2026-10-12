// chord.cpp —— 和弦图实现
#include "termchart/graph/diagrams/chord.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/graph/shape/shape.hpp"
#include "termchart/util/wcwidth.hpp"
#include "internal.hpp"

namespace termchart::graph {
namespace {

constexpr double kTwoPi = 6.283185307179586476925287;

constexpr double kMaxSamples = 8192.0;

// 采样二次贝塞尔并去重成格序列（步数按弦长估计，一格约 4 个样本）。
std::vector<Point> quadratic_cells(Point p0, Point p1, Point p2) {
    const double chord = std::hypot(p2.x - p0.x, p2.y - p0.y);
    std::size_t steps = 2;
    if (chord > 0.5) {
        const double raw = std::ceil(chord * 4.0);
        steps = raw >= kMaxSamples ? static_cast<std::size_t>(kMaxSamples)
                                   : static_cast<std::size_t>(raw);
    }
    std::vector<Point> cells;
    cells.reserve(steps + 1);
    for (std::size_t k = 0; k <= steps; ++k) {
        const double t = static_cast<double>(k) / static_cast<double>(steps);
        const double u = 1.0 - t;
        const Point s{u * u * p0.x + 2.0 * u * t * p1.x + t * t * p2.x,
                      u * u * p0.y + 2.0 * u * t * p1.y + t * t * p2.y};
        const Point c{std::round(s.x), std::round(s.y)};
        if (cells.empty() || cells.back() != c) {
            cells.push_back(c);
        }
    }
    return cells;
}

}  // namespace

Rect render_chord(const std::vector<ChordEntity>& entities,
                  const std::vector<ChordLink>& links, canvas::Canvas& canvas,
                  const ChordOptions& opts) {
    const std::size_t n = entities.size();
    if (n == 0) {
        return Rect{};
    }
    const double ry = std::max(2.0, opts.radius);
    const double rx = ry * 2.0;  // 字符格 1:2 宽高比补偿：横向半径加倍才圆

    // 圆心给标签留出外侧空间：左侧标签右对齐到锚点左 1 格，x 最小到
    // max_label - w >= 0，刚好不越画布左界；越界部分仍由 canvas 裁剪。
    double max_label = 1.0;
    for (const ChordEntity& e : entities) {
        max_label = std::max(max_label, static_cast<double>(util::display_width(e.label)));
    }
    const Point center{rx + 3.0 + max_label, ry + 2.0};

    // 1) 环：复用 Ellipse 的逐行解析轮廓。
    const Rect ring_box{center.x - rx, center.y - ry, rx * 2.0 + 1.0, ry * 2.0 + 1.0};
    if (opts.draw_ring) {
        draw_shape(canvas, ShapeKind::Ellipse, ring_box, opts.ring_style);
    }
    Rect bounds = ring_box;

    // 2) 实体锚角：第一个实体在正上方，顺时针等分（屏幕坐标 y 向下）。
    std::vector<double> angle(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        angle[i] = -kTwoPi / 4.0 +
                   (static_cast<double>(i) + 0.5) * kTwoPi / static_cast<double>(n);
    }

    // 3) 弦：锚点沿切向展开成平行弧，值越大带越宽。
    double max_value = 0.0;
    for (const ChordLink& l : links) {
        if (l.from < n && l.to < n && l.from != l.to && std::isfinite(l.value)) {
            max_value = std::max(max_value, l.value);
        }
    }
    const int max_band = std::max(1, opts.max_band);
    for (const ChordLink& l : links) {
        if (l.from >= n || l.to >= n || l.from == l.to) {
            continue;
        }
        if (!(l.value > 0.0) || !std::isfinite(l.value)) {
            continue;
        }
        int band = 1;
        if (max_band > 1 && max_value > 0.0) {
            const double raw =
                1.0 + l.value / max_value * static_cast<double>(max_band - 1);
            band = std::clamp(static_cast<int>(std::lround(raw)), 1, max_band);
        }
        const double af = angle[l.from];
        const double at = angle[l.to];
        const Point pf{center.x + rx * std::cos(af), center.y + ry * std::sin(af)};
        const Point pt{center.x + rx * std::cos(at), center.y + ry * std::sin(at)};
        // 切向单位向量（角度增加的方向，即屏幕上的顺时针）。
        const Point tf{-std::sin(af), std::cos(af)};
        const Point tt{-std::sin(at), std::cos(at)};
        const Point mid{(pf.x + pt.x) / 2.0, (pf.y + pt.y) / 2.0};
        // 控制点从中点拉向圆心：弧带整体凹进环内，是和弦图的典型观感。
        const Point ctrl{mid.x + (center.x - mid.x) * 0.45,
                         mid.y + (center.y - mid.y) * 0.45};
        for (int j = 0; j < band; ++j) {
            const double off =
                static_cast<double>(j) - static_cast<double>(band - 1) / 2.0;
            const Point q0{pf.x + tf.x * off, pf.y + tf.y * off};
            const Point q2{pt.x + tt.x * off, pt.y + tt.y * off};
            const Point q1{ctrl.x + (tf.x + tt.x) * off * 0.5,
                           ctrl.y + (tf.y + tt.y) * off * 0.5};
            const std::vector<Point> cells = quadratic_cells(q0, q1, q2);
            for (std::size_t k = 1; k < cells.size(); ++k) {
                canvas.draw_line(cells[k - 1], cells[k],
                                 detail::segment_char(cells[k - 1], cells[k]),
                                 opts.band_style);
            }
        }
    }

    // 4) 标签：锚点向外偏移，按方位对齐（右侧左对齐、右侧右对齐、上下居中）。
    for (std::size_t i = 0; i < n; ++i) {
        const std::string& label = entities[i].label;
        if (label.empty()) {
            continue;
        }
        const double a = angle[i];
        const double c = std::cos(a);
        const double w = static_cast<double>(util::display_width(label));
        const Point anchor{center.x + (rx + 2.0) * c, center.y + (ry + 1.0) * std::sin(a)};
        double x = anchor.x - w / 2.0;
        if (c > 0.3) {
            x = anchor.x + 1.0;
        } else if (c < -0.3) {
            x = anchor.x - 1.0 - w;
        }
        const double y = std::round(anchor.y);
        canvas.draw_text(Point{std::round(x), y}, label, opts.label_style);
        bounds = bounds.union_with(Rect{std::round(x), y, w, 1.0});
    }
    return bounds;
}

}  // namespace termchart::graph
