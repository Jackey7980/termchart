// auto_margin.hpp —— 由内容反推四边留白
//
// 终端图表的留白不是美学问题，而是"标签放得下吗"的问题：y 刻度标签占左边、
// x 刻度标签占下边、标题占上边、图例占右边。把这几项**显式建模**成输入再算，
// 比在 chart 里散落一堆硬编码数字（然后每次换数据就压字）可靠得多。
//
// 口径：所有留白**向上取整到整格**（字符网格上没有半格），并保证每边至少为 padding。
#ifndef TERMCHART_LAYOUT_AUTO_MARGIN_HPP
#define TERMCHART_LAYOUT_AUTO_MARGIN_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "termchart/export.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::layout {

struct MarginInputs {
    std::uint32_t y_tick_label_width = 0;  // y 刻度标签最大显示宽度（列）
    std::uint32_t y_tick_label_gap = 1;    // 标签与轴线之间的空隙（列）
    std::uint32_t x_tick_label_height = 0;  // x 刻度标签占的行数（通常 1）
    std::uint32_t x_tick_label_gap = 1;     // 标签与轴线之间的空隙（行）
    // x 刻度标签最宽宽度：最右一个标签是**居中**在刻度上的，所以右侧只需预留一半。
    std::uint32_t x_tick_label_width = 0;
    std::uint32_t title_lines = 0;    // 标题占的行数
    std::uint32_t legend_width = 0;   // 图例块宽度（列）；0 表示不画图例
    std::uint32_t legend_gap = 1;     // 图例与绘图区之间的空隙
    std::uint32_t axis_label_rows = 0;  // x 轴名占的行数
    double padding = 1.0;             // 四边最小留白
};

// 由建模后的输入算四边留白。
[[nodiscard]] TERMCHART_API Margin compute_margins(const MarginInputs& inputs) noexcept;

// 便捷：直接从刻度标签文本算（取最大**显示宽度**，宽字符按 2 列计）。
// base 里的其余字段（标题行数、图例宽度等）照常生效；标签相关的三个字段被覆盖。
[[nodiscard]] TERMCHART_API Margin compute_margins_for(
    const std::vector<std::string>& y_tick_labels,
    const std::vector<std::string>& x_tick_labels, const MarginInputs& base = {});

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_AUTO_MARGIN_HPP
