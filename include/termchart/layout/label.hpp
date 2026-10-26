// label.hpp —— 轴名标签（轴下方的 x 轴名 / 轴左侧的 y 轴名）
#ifndef TERMCHART_LAYOUT_LABEL_HPP
#define TERMCHART_LAYOUT_LABEL_HPP

#include <string_view>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::layout {

class Sink;

// 布局主体：只算位置，落笔交给 sink。
TERMCHART_API void draw_axis_label(Sink& sink, std::string_view text, const Rect& rect,
                                   Side side, HAlign halign = HAlign::Center,
                                   const Style& style = Style{});

// 兼容入口：内部转成 CanvasSink。
TERMCHART_API void draw_axis_label(canvas::Canvas& canvas, std::string_view text,
                                   const Rect& rect, Side side,
                                   HAlign halign = HAlign::Center,
                                   const Style& style = Style{});

// ── 轴标签抽稀（纯算法，无绘制）────────────────────────────────────────────
// 刻度过密时标签会互相覆盖：给定刻度在屏幕上的位置（单元格坐标，升序）与最小标签
// 间隔 min_gap_cells，返回应保留的刻度下标。贪心：从左到右，跳过与上一个保留项
// 间距不足 min_gap_cells 的项（始终保留第一个）。
[[nodiscard]] TERMCHART_API std::vector<std::size_t> thin_tick_labels(
    const std::vector<double>& positions, double min_gap_cells);

// 重载：显式给出每个标签宽度（单元格数），按"上一标签右沿 → 当前标签左沿"间距判定，
// 适合标签宽度不等（如 "03-02 08:00" 与 "1e9" 跨度极大）的情况。
[[nodiscard]] TERMCHART_API std::vector<std::size_t> thin_tick_labels(
    const std::vector<double>& positions, const std::vector<std::size_t>& widths,
    double min_gap_cells);

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_LABEL_HPP
