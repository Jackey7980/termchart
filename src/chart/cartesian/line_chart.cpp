// line_chart.cpp —— 笛卡尔折线图实现
#include "termchart/chart/cartesian/line_chart.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/chart/scene_sink.hpp"
#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/sink.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"
#include "termchart/scene/scene.hpp"

namespace termchart::chart {

void LineChart::draw(DrawContext ctx) {
    double xmin, xmax, ymin, ymax;
    data_bounds(series_, xmin, xmax, ymin, ymax);
    if (!std::isfinite(xmin)) {  // 无数据：空域
        xmin = 0.0;
        xmax = 1.0;
        ymin = 0.0;
        ymax = 1.0;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, xmin, xmax, ymin, ymax, auto_domain_,
                      include_zero_, y_domain_);

    // 显式刻度优先：调用方知道 x 的语义（例如时间），能给出日历对齐的位置；
    // 否则退回 nice 值。这里只做"用哪一份"的选择，不掺任何语义判断。
    const std::vector<double> xticks =
        x_ticks_.empty() ? scale::ticks_for(area_.x_scale()) : x_ticks_;
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());

    // 上下文开关在这里落地：默认全开，导出/GUI 侧可以只要数据不要轴。
    const Style axis_style = ctx.axis_style;
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        if (x_tick_formatter_) {
            std::vector<std::string> labels;
            labels.reserve(xticks.size());
            for (const double t : xticks) {
                labels.push_back(x_tick_formatter_(t));
            }
            layout::draw_axis_x(ctx.canvas, area_, xticks, labels, "x", axis_style);
        } else {
            layout::draw_axis_x(ctx.canvas, area_, xticks, "x", axis_style);
        }
        layout::draw_axis_y(ctx.canvas, area_, yticks, "y", axis_style);
    }

    for (const Series& s : series_) {
        const Style st = Style{}.set_fg(s.color);
        // 动画进度按系列名查：没登记 → 1.0（全画），登记过则从左向右揭开。
        // 注意**域不跟着进度变**：轴始终按完整数据算，否则画面会一边长一边缩放。
        const double progress = progress_of(s.name);
        if (progress <= 0.0 || s.points.empty()) {
            continue;
        }
        // 多数输入本已按 x 升序：已排序则跳过拷贝 + 排序，避免动画 redraw 每帧
        // O(N) 拷贝 + O(N log N) 排序（域不跟着进度变，见上方注释）。
        const std::vector<Point>* ppts = &s.points;
        std::vector<Point> pts_buf;
        if (!std::is_sorted(s.points.begin(), s.points.end(),
                            [](const Point& a, const Point& b) { return a.x < b.x; })) {
            pts_buf = s.points;
            std::sort(pts_buf.begin(), pts_buf.end(),
                      [](const Point& a, const Point& b) { return a.x < b.x; });
            ppts = &pts_buf;
        }
        const std::vector<Point>& pts = *ppts;

        // 进度折算成"段数"：p=0.5 时最后一段只画一半，避免整段跳的顿挫。
        const std::size_t segs = pts.size() - 1;
        const double scaled = static_cast<double>(segs) * progress;
        const std::size_t full = std::min(static_cast<std::size_t>(scaled), segs);
        for (std::size_t i = 1; i <= full; ++i) {
            const Point a = area_.cell_of(pts[i - 1].x, pts[i - 1].y);
            const Point b = area_.cell_of(pts[i].x, pts[i].y);
            ctx.canvas.draw_line(a, b, kLineChar, st);
        }
        if (full < segs) {
            const double frac = scaled - static_cast<double>(full);
            if (frac > 0.0) {
                // 在**数据坐标**里插值再投影：比例尺非线性时格坐标插值会偏。
                const Point& p0 = pts[full];
                const Point& p1 = pts[full + 1];
                const Point mid{p0.x + (p1.x - p0.x) * frac, p0.y + (p1.y - p0.y) * frac};
                ctx.canvas.draw_line(area_.cell_of(p0.x, p0.y), area_.cell_of(mid.x, mid.y),
                                     kLineChar, st);
            }
        }

        // 数据点标记只画进度已经过的部分（含部分段的起点）。
        const std::size_t markers = std::min(full + 1, pts.size());
        for (std::size_t i = 0; i < markers; ++i) {
            ctx.canvas.put(area_.cell_of(pts[i].x, pts[i].y), kMarkerChar, st);
        }
    }

    draw_legend_for(ctx.canvas, series_, area_, show_legend_ && ctx.draw_legend);
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, area_.inner_rect());
    }
}

bool LineChart::to_scene(scene::Scene& out, std::uint32_t width, std::uint32_t height) const {
    double xmin, xmax, ymin, ymax;
    data_bounds(series_, xmin, xmax, ymin, ymax);
    if (!std::isfinite(xmin)) {
        xmin = 0.0;
        xmax = 1.0;
        ymin = 0.0;
        ymax = 1.0;
    }
    // 与 draw() 走**同一个** make_area：布局只有一份，"第几列是刻度"不可能
    // 在两条通路上算出不同答案。
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(width, height, m, xmin, xmax, ymin, ymax, auto_domain_, include_zero_,
                      y_domain_);

    const std::vector<double> xticks =
        x_ticks_.empty() ? scale::ticks_for(area_.x_scale()) : x_ticks_;
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());

    SceneSink sink(out);

    layout::draw_grid(sink, area_, xticks, yticks,
                      Style{}.set_fg(default_color(DefaultColor::Muted)));
    if (x_tick_formatter_) {
        std::vector<std::string> labels;
        labels.reserve(xticks.size());
        for (const double t : xticks) {
            labels.push_back(x_tick_formatter_(t));
        }
        layout::draw_axis_x(sink, area_, xticks, labels, "x", Style{});
    } else {
        layout::draw_axis_x(sink, area_, xticks, "x", Style{});
    }
    layout::draw_axis_y(sink, area_, yticks, "y", Style{});

    scene::SceneBuilder& builder = sink.builder();
    std::uint32_t group = 0;
    for (const Series& s : series_) {
        if (s.points.empty()) {
            continue;
        }
        // 与 draw() 同源：已按 x 升序则跳过拷贝 + 排序。
        const std::vector<Point>* ppts = &s.points;
        std::vector<Point> pts_buf;
        if (!std::is_sorted(s.points.begin(), s.points.end(),
                            [](const Point& a, const Point& b) { return a.x < b.x; })) {
            pts_buf = s.points;
            std::sort(pts_buf.begin(), pts_buf.end(),
                      [](const Point& a, const Point& b) { return a.x < b.x; });
            ppts = &pts_buf;
        }
        const std::vector<Point>& pts = *ppts;

        // 这里出的是**完整几何**，不看进度：动画由 `apply_animation` 在
        // Scene 层统一施加（见 chart/scene_animation.hpp）。两条理由：
        //   1. 一份 Scene 可以反复截断出多帧，不必每帧重算轴/刻度/文本；
        //   2. 动画逻辑只有一个实现点，新增图种不用再抄一遍截断代码。
        // 代价是截断发生在屏幕空间，非线性比例尺下与"数据坐标插值"差一点点。
        builder.begin_group(group);

        // 折线走矢量：终端侧由 render_scene 光栅成盲文，比逐格 • 更连续。
        builder.stroke(s.color, 1.0);
        builder.move_to(area_.cell_of(pts[0].x, pts[0].y));
        for (std::size_t i = 1; i < pts.size(); ++i) {
            builder.line_to(area_.cell_of(pts[i].x, pts[i].y));
        }

        // 标记点是符号，不是几何：矢量小圆会被半块光栅成实心方块，那就不是 ● 了。
        const Style marker_style = Style{}.set_fg(s.color);
        for (const Point& p : pts) {
            sink.put(area_.cell_of(p.x, p.y), kMarkerChar, marker_style);
        }

        builder.end_group(group);
        ++group;
    }

    draw_legend_for(sink, series_, area_, show_legend_);
    if (!title_.empty()) {
        layout::draw_title(sink, title_, area_.inner_rect());
    }
    return true;
}

void LineChart::animation_groups(std::vector<GroupProgress>& out) const {
    // 组号分配必须与 to_scene 的 begin_group **逐条对齐**：空系列在那边被
    // 跳过（不占组号），这里也必须跳过——否则第二个系列会拿到组 0 的进度。
    std::uint32_t group = 0;
    for (const Series& s : series_) {
        if (s.points.empty()) {
            continue;
        }
        out.push_back(GroupProgress{group, progress_of(s.name)});
        ++group;
    }
}

}  // namespace termchart::chart
