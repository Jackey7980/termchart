// parallel.cpp —— 平行坐标实现（逐维归一化 → 竖轴 → 跨轴折线）
#include "termchart/chart/cartesian/parallel.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

#include "termchart/canvas/canvas.hpp"
#include "termchart/layout/legend.hpp"
#include "termchart/layout/title.hpp"

namespace termchart::chart {
namespace {

// 观测线轮换用的 16 色调色板：避开黑白灰与暗灰，保证线与轴可分。
Color line_color(std::size_t i) noexcept {
    static constexpr std::array<int, 6> kPalette{1, 3, 2, 5, 4, 6};
    return Color::ansi16(static_cast<std::uint8_t>(kPalette[i % kPalette.size()]));
}

}  // namespace

void ParallelChart::set_axes(std::vector<std::string> names,
                             std::vector<std::vector<double>> observations) {
    names_ = std::move(names);
    observations_ = std::move(observations);
}

void ParallelChart::draw(DrawContext ctx) {
    const auto k = static_cast<int>(names_.size());
    if (k < 2) {
        return;  // 少于两个维度不构成平行坐标
    }
    // 只保留"长度匹配且全有限"的观测：部分缺失的行在轴间无法连线。
    std::vector<std::vector<double>> rows;
    rows.reserve(observations_.size());
    for (const std::vector<double>& r : observations_) {
        if (r.size() != names_.size()) {
            continue;
        }
        bool ok = true;
        for (const double v : r) {
            ok = ok && std::isfinite(v);
        }
        if (ok) {
            rows.push_back(r);
        }
    }

    // 逐维 min/max（只统计有效行）。
    std::vector<double> dmin(static_cast<std::size_t>(k),
                             std::numeric_limits<double>::infinity());
    std::vector<double> dmax(static_cast<std::size_t>(k),
                             -std::numeric_limits<double>::infinity());
    for (const std::vector<double>& r : rows) {
        for (int j = 0; j < k; ++j) {
            const double v = r[static_cast<std::size_t>(j)];
            dmin[static_cast<std::size_t>(j)] = std::min(dmin[static_cast<std::size_t>(j)], v);
            dmax[static_cast<std::size_t>(j)] = std::max(dmax[static_cast<std::size_t>(j)], v);
        }
    }
    for (int j = 0; j < k; ++j) {
        if (!(dmax[static_cast<std::size_t>(j)] >
              dmin[static_cast<std::size_t>(j)])) {
            dmin[static_cast<std::size_t>(j)] -= 0.5;  // 常值维度给单位跨度
            dmax[static_cast<std::size_t>(j)] += 0.5;
        }
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, 0.0, 1.0, 0.0, 1.0, false, false);
    const Rect inner = area_.inner_rect();

    // 竖轴列坐标：k 条轴均分内框宽度（首尾贴边）。
    std::vector<double> axis_x(static_cast<std::size_t>(k), 0.0);
    for (int j = 0; j < k; ++j) {
        axis_x[static_cast<std::size_t>(j)] =
            inner.x + static_cast<double>(j) * (inner.w - 1.0) /
                          static_cast<double>(k - 1);
    }

    if (options_.show_axes) {
        const Style axis_st = ctx.axis_style.fg().is_none()
                                  ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                  : ctx.axis_style;
        for (int j = 0; j < k; ++j) {
            const double ax = axis_x[static_cast<std::size_t>(j)];
            ctx.canvas.draw_line(Point{ax, inner.y}, Point{ax, inner.bottom() - 1.0},
                                 U'\u2502', axis_st);  // │
            ctx.canvas.draw_text(Point{ax, std::max(0.0, inner.y - 1.0)},
                                 names_[static_cast<std::size_t>(j)], Style{});
        }
    }

    for (std::size_t r = 0; r < rows.size(); ++r) {
        const std::vector<double>& obs = rows[r];
        const Style st = Style{}.set_fg(line_color(r));
        double px = 0.0;
        double py = 0.0;
        for (int j = 0; j < k; ++j) {
            const double t = (obs[static_cast<std::size_t>(j)] -
                              dmin[static_cast<std::size_t>(j)]) /
                             (dmax[static_cast<std::size_t>(j)] -
                              dmin[static_cast<std::size_t>(j)]);
            const double x = axis_x[static_cast<std::size_t>(j)];
            const double y = inner.bottom() - 1.0 - t * (inner.h - 1.0);
            if (j > 0) {
                ctx.canvas.draw_line(Point{px, py}, Point{x, y}, kLineChar, st);
            }
            if (options_.show_markers) {
                ctx.canvas.put(Point{x, y}, kMarkerChar, st);
            }
            px = x;
            py = y;
        }
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

}  // namespace termchart::chart
