// gantt.cpp —— 甘特图实现（任务行 + 时间条 + 行标签）
#include "termchart/chart/cartesian/gantt.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"
#include "termchart/chart/scene_sink.hpp"

namespace termchart::chart {
namespace {

// kBarChar（█ 时间条）沿用 chart.hpp 的共享常量。

// 按 UTF-8 码点截断（前 max_cp 个码点），绝不撕裂多字节序列。
std::string_view truncate_utf8(std::string_view s, std::size_t max_cp) noexcept {
    std::size_t cp = 0;
    std::size_t end = 0;
    while (end < s.size()) {
        // UTF-8 前导字节：非 10xxxxxx（& 0xC0 != 0x80）即为新码点起点。
        if ((static_cast<unsigned char>(s[end]) & 0xC0) != 0x80) {
            if (cp == max_cp) {
                return s.substr(0, end);
            }
            ++cp;
        }
        ++end;
    }
    return s;
}

}  // namespace

void GanttChart::draw(DrawContext ctx) {
    // 只保留 [start, end] 有限且 end>=start 的任务；其余剔除。
    std::vector<Task> tasks;
    tasks.reserve(tasks_.size());
    for (const Task& t : tasks_) {
        if (std::isfinite(t.start) && std::isfinite(t.end) && t.end >= t.start) {
            tasks.push_back(t);
        }
    }
    if (tasks.empty()) {
        return;
    }

    double tmin = tasks.front().start;
    double tmax = tasks.front().end;
    for (const Task& t : tasks) {
        tmin = std::min(tmin, t.start);
        tmax = std::max(tmax, t.end);
    }
    if (!(tmax > tmin)) {
        tmax = tmin + 1.0;  // 全部同时刻：给单位时长避免零宽域
    }

    const auto n = static_cast<double>(tasks.size());
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, tmin, tmax, 0.0, n, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const Style axis_style = ctx.axis_style;
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        // 只画垂直时间网格线（空 y 刻度 = 无横线）。
        layout::draw_grid(ctx.canvas, area_, xticks, {}, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "time", axis_style);
    }

    const Rect inner = area_.inner_rect();
    const double bh = std::clamp(inner.h / n * 0.6, 1.0, 3.0);

    for (std::size_t r = 0; r < tasks.size(); ++r) {
        const Task& t = tasks[r];
        const double row_c =
            inner.y + (static_cast<double>(r) + 0.5) * inner.h / n;
        const double x0 = area_.x_scale().forward(t.start);
        const double x1 = area_.x_scale().forward(t.end);
        const Style st = Style{}.set_fg(opts_.bar);
        ctx.canvas.fill_rect(Rect{x0, row_c - bh / 2.0, std::max(1.0, x1 - x0), bh},
                             kBarChar, st);

        if (opts_.show_labels) {
            // 标签写在左侧留白里：最多占 margin_left-2 列，超出按码点截断。
            const std::size_t label_cols =
                margin_left_ >= 2.0 ? static_cast<std::size_t>(margin_left_) - 2 : 0;
            if (label_cols > 0) {
                const Style lst = Style{}.set_fg(opts_.bar);
                ctx.canvas.draw_text(
                    Point{0.0, std::floor(row_c)},
                    truncate_utf8(t.name, label_cols), lst);
            }
        }
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

bool GanttChart::to_scene(scene::Scene& out, std::uint32_t width,
                          std::uint32_t height) const {
    // 与 draw() 同源：只保留 [start, end] 有限且 end>=start 的任务。
    std::vector<Task> tasks;
    tasks.reserve(tasks_.size());
    for (const Task& t : tasks_) {
        if (std::isfinite(t.start) && std::isfinite(t.end) && t.end >= t.start) {
            tasks.push_back(t);
        }
    }
    if (tasks.empty()) {
        return true;
    }

    double tmin = tasks.front().start;
    double tmax = tasks.front().end;
    for (const Task& t : tasks) {
        tmin = std::min(tmin, t.start);
        tmax = std::max(tmax, t.end);
    }
    if (!(tmax > tmin)) {
        tmax = tmin + 1.0;  // 全部同时刻：给单位时长避免零宽域
    }

    const auto n = static_cast<double>(tasks.size());
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(width, height, m, tmin, tmax, 0.0, n, false, false);

    // 仅时间网格（无 y 刻度 → 无横线）+ 时间轴；任务行是分类、不画 y 轴，与 draw 一致。
    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    SceneSink sink(out);
    layout::draw_grid(sink, area_, xticks, {},
                      Style{}.set_fg(default_color(DefaultColor::Muted)));
    layout::draw_axis_x(sink, area_, xticks, "time", Style{});

    const Rect inner = area_.inner_rect();
    const double bh = std::clamp(inner.h / n * 0.6, 1.0, 3.0);
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);
    for (std::size_t r = 0; r < tasks.size(); ++r) {
        const Task& t = tasks[r];
        const double row_c =
            inner.y + (static_cast<double>(r) + 0.5) * inner.h / n;
        const double x0 = area_.x_scale().forward(t.start);
        const double x1 = area_.x_scale().forward(t.end);
        const Style st = Style{}.set_fg(opts_.bar);
        // 时间条 → 矢量矩形（RectOp + FillOp）。
        b.fill(st.fg());
        b.rect(Rect{x0, row_c - bh / 2.0, std::max(1.0, x1 - x0), bh});

        if (opts_.show_labels) {
            // 行标签写在左侧留白里，超出按码点截断（与 draw 同源）。
            const std::size_t label_cols =
                margin_left_ >= 2.0 ? static_cast<std::size_t>(margin_left_) - 2 : 0;
            if (label_cols > 0) {
                const Style lst = Style{}.set_fg(opts_.bar);
                sink.text(Point{0.0, std::floor(row_c)},
                          truncate_utf8(t.name, label_cols), lst);
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
