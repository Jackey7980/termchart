// legend.hpp —— 图例：色块 + 名称，水平排布在给定矩形内
#ifndef TERMCHART_LAYOUT_LEGEND_HPP
#define TERMCHART_LAYOUT_LEGEND_HPP

#include <span>
#include <string>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::layout {

struct LegendEntry {
    std::string name;
    Color color = Color::ansi16(7);
};

// 图例布局选项。wrap=true 时超出 max_width_cells 自动换行（多行向下堆叠）。
struct LegendOptions {
    HAlign halign = HAlign::Left;
    bool wrap = false;
    double max_width_cells = 0.0;  // 0 = 用 rect 宽度
};

class Sink;

TERMCHART_API void draw_legend(Sink& sink, std::span<const LegendEntry> entries, const Rect& rect,
                               HAlign halign = HAlign::Left,
                               const Style& base = Style{});

TERMCHART_API void draw_legend(Sink& sink, std::span<const LegendEntry> entries, const Rect& rect,
                               const LegendOptions& opts, const Style& base = Style{});

TERMCHART_API void draw_legend(canvas::Canvas& canvas, std::span<const LegendEntry> entries,
                               const Rect& rect, HAlign halign = HAlign::Left,
                               const Style& base = Style{});

TERMCHART_API void draw_legend(canvas::Canvas& canvas, std::span<const LegendEntry> entries,
                               const Rect& rect, const LegendOptions& opts,
                               const Style& base = Style{});

// ── 图例行分组（纯算法，无绘制）────────────────────────────────────────────
// 给定图例项与单行最大宽度（单元格数），贪心地把项分组到若干行，保证每行宽度不超
// max_width_cells，返回每行包含的项下标。任一单项超过 max_width_cells 也独占一行
// （不截断）。用于 draw_legend(wrap=true) 的内部计算，也可被调用方独立测试/复用。
[[nodiscard]] TERMCHART_API std::vector<std::vector<std::size_t>> legend_rows(
    std::span<const LegendEntry> entries, double max_width_cells);

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_LEGEND_HPP
