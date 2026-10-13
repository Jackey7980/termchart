// contour.cpp —— 等高线实现（marching squares 逐级提取折线段）
#include "termchart/chart/cartesian/contour.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

inline constexpr char32_t kContourChar = U'\u00B7';  // · 等值线

// 线性插值出等值线与一条边的交点（数据坐标）。两端值夹不住 level 时返回 nullopt。
struct Seg {
    Point a{};
    Point b{};
};

bool edge_crossing(double v0, double v1, double level, double x0, double y0, double x1,
                   double y1, Point& out) noexcept {
    const double d = v1 - v0;
    if (d == 0.0) {
        return false;
    }
    const double t = (level - v0) / d;
    if (!(t >= 0.0 && t <= 1.0)) {
        return false;
    }
    out = Point{x0 + (x1 - x0) * t, y0 + (y1 - y0) * t};
    return true;
}

}  // namespace

void ContourChart::draw(DrawContext ctx) {
    // 场裁成矩形：列数取各行最小值；行数/列数 <2 无法提取等值线。
    if (rows_.size() < 2) {
        return;
    }
    std::size_t cols = rows_.front().size();
    for (const std::vector<double>& row : rows_) {
        cols = std::min(cols, row.size());
    }
    if (cols < 2) {
        return;
    }
    // 场内出现非有限值会让插值与比较失真：包含非有限值的格直接跳过（局部空洞）。
    const std::size_t nrows = rows_.size();

    double vmin = 0.0;
    double vmax = 0.0;
    bool have = false;
    for (const std::vector<double>& row : rows_) {
        for (std::size_t i = 0; i < cols && i < row.size(); ++i) {
            if (std::isfinite(row[i])) {
                vmin = have ? std::min(vmin, row[i]) : row[i];
                vmax = have ? std::max(vmax, row[i]) : row[i];
                have = true;
            }
        }
    }
    if (!have) {
        return;
    }

    // 等值线级：显式给定则用之（剔除非有限），否则在值域内均分、不含端点。
    std::vector<double> levels;
    if (!opts_.levels.empty()) {
        for (double l : opts_.levels) {
            if (std::isfinite(l)) {
                levels.push_back(l);
            }
        }
    } else {
        const int count = std::clamp(opts_.level_count, 1, 16);
        levels.reserve(static_cast<std::size_t>(count));
        for (int k = 1; k <= count; ++k) {
            levels.push_back(vmin + (vmax - vmin) * static_cast<double>(k) /
                                        static_cast<double>(count + 1));
        }
    }
    if (levels.empty()) {
        return;
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, 0.0, static_cast<double>(cols - 1), 0.0,
                      static_cast<double>(nrows - 1), false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    const Style axis_style = ctx.axis_style;
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "col", axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "row", axis_style);
    }

    const Style st = Style{}.set_fg(opts_.color);
    // 场坐标：x=列、y=行（row 0 在上），与 PlotArea 的"y 向下增长"同构，
    // 于是行 j 直接映射为数据 y=j。
    const auto x_of = [&](std::size_t c) noexcept {
        return static_cast<double>(c);
    };
    const auto y_of = [&](std::size_t r) noexcept {
        return static_cast<double>(r);
    };

    for (double level : levels) {
        for (std::size_t j = 0; j + 1 < nrows; ++j) {
            const std::vector<double>& row_top = rows_[j];
            const std::vector<double>& row_bot = rows_[j + 1];
            if (row_top.size() < cols || row_bot.size() < cols) {
                continue;
            }
            for (std::size_t i = 0; i + 1 < cols; ++i) {
                const double v00 = row_top[i];
                const double v10 = row_top[i + 1];
                const double v01 = row_bot[i];
                const double v11 = row_bot[i + 1];
                if (!std::isfinite(v00) || !std::isfinite(v10) || !std::isfinite(v01) ||
                    !std::isfinite(v11)) {
                    continue;
                }
                const int case_bits = (v00 > level ? 8 : 0) | (v10 > level ? 4 : 0) |
                                      (v11 > level ? 2 : 0) | (v01 > level ? 1 : 0);
                if (case_bits == 0 || case_bits == 15) {
                    continue;
                }
                const double x0 = x_of(i);
                const double x1 = x_of(i + 1);
                const double y0 = y_of(j);
                const double y1 = y_of(j + 1);
                Point top{};
                Point bottom{};
                Point left{};
                Point right{};
                const bool has_top = edge_crossing(v00, v10, level, x0, y0, x1, y0, top);
                const bool has_bottom =
                    edge_crossing(v01, v11, level, x0, y1, x1, y1, bottom);
                const bool has_left = edge_crossing(v00, v01, level, x0, y0, x0, y1, left);
                const bool has_right =
                    edge_crossing(v10, v11, level, x1, y0, x1, y1, right);
                Seg segs[2] = {};
                int nseg = 0;
                switch (case_bits) {
                    case 1:
                    case 14:
                        if (has_left && has_bottom) segs[nseg++] = Seg{left, bottom};
                        break;
                    case 2:
                    case 13:
                        if (has_bottom && has_right) segs[nseg++] = Seg{bottom, right};
                        break;
                    case 3:
                    case 12:
                        if (has_left && has_right) segs[nseg++] = Seg{left, right};
                        break;
                    case 4:
                    case 11:
                        if (has_top && has_right) segs[nseg++] = Seg{top, right};
                        break;
                    case 6:
                    case 9:
                        if (has_top && has_bottom) segs[nseg++] = Seg{top, bottom};
                        break;
                    case 7:
                    case 8:
                        if (has_left && has_top) segs[nseg++] = Seg{left, top};
                        break;
                    case 5:  // 鞍点：默认取"左上 + 右下"一剖
                        if (has_left && has_top) segs[nseg++] = Seg{left, top};
                        if (has_bottom && has_right) segs[nseg++] = Seg{bottom, right};
                        break;
                    case 10:  // 鞍点：另一剖
                        if (has_top && has_right) segs[nseg++] = Seg{top, right};
                        if (has_left && has_bottom) segs[nseg++] = Seg{left, bottom};
                        break;
                    default:
                        break;
                }
                for (int s = 0; s < nseg; ++s) {
                    ctx.canvas.draw_line(area_.cell_of(segs[s].a.x, segs[s].a.y),
                                         area_.cell_of(segs[s].b.x, segs[s].b.y),
                                         kContourChar, st);
                }
            }
        }
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, area_.inner_rect());
    }
}

}  // namespace termchart::chart
