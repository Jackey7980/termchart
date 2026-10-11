// context.hpp —— 图表绘制上下文（DrawContext）
//
// 从 `chart.hpp` 独立出来（一个上下文、一个基类、一个系列类型，各自成文件）：
// 上下文描述"这一次绘制用什么开关和样式"，与 `Chart` 基类解耦后，GUI 侧与
// 文档导出路径可以构造自己的上下文（例如不画轴、不画标题、只用网格），
// 而不必继承或伪造一个 Chart。
//
// 所有开关默认与既有行为一致（全开），因此老调用点只需 `DrawContext{canvas}`。
#ifndef TERMCHART_CHART_CONTEXT_HPP
#define TERMCHART_CHART_CONTEXT_HPP

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

struct DrawContext {
    canvas::Canvas& canvas;

    // 画什么（默认全开）。
    bool draw_title = true;
    bool draw_axes = true;
    bool draw_grid = true;
    bool draw_legend = true;

    // 样式覆盖：`fg()` 为 None 时图表用各自的默认配色（网格是暗灰、轴是默认前景）。
    Style grid_style{};
    Style axis_style{};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CONTEXT_HPP
