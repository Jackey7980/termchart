// axis.hpp —— 坐标轴绘制（左 y 轴 / 下 x 轴）：轴体 + 刻度 + 数字标签 + 轴名
#ifndef TERMCHART_LAYOUT_AXIS_HPP
#define TERMCHART_LAYOUT_AXIS_HPP

#include <string>
#include <string_view>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::layout {

class Sink;

// ── 布局主体（落笔交给 sink）─────────────────────────────────────────────
// 下 x 轴：在 inner 底边画横线，向上长出刻度并标注数值；label 居中置于轴下方。
TERMCHART_API void draw_axis_x(Sink& sink, const PlotArea& area,
                               const std::vector<double>& ticks, std::string_view label = {},
                               const Style& style = Style{});

// 下 x 轴（显式标签版）：ticks 决定**刻度位置**，labels 决定**标签文本**。
// 标签按 ticks 的下标一一对应；labels 短于 ticks 的位置回退 format_tick。
// 分工刻意如此：layout 不认识"时间"这类语义，只负责画；"epoch 秒 → 日期字符串"
// 由调用方（chart 的 formatter / autoviz）格式化后传进来。
TERMCHART_API void draw_axis_x(Sink& sink, const PlotArea& area,
                               const std::vector<double>& ticks,
                               const std::vector<std::string>& labels,
                               std::string_view label = {}, const Style& style = Style{});

// 左 y 轴：在 inner 左边画竖线，向右长出刻度并标注数值；label 置于轴左侧。
TERMCHART_API void draw_axis_y(Sink& sink, const PlotArea& area,
                               const std::vector<double>& ticks, std::string_view label = {},
                               const Style& style = Style{});

// 右 y 轴（双 Y 轴 / 次坐标轴）：在 inner 右边画竖线，向左长出刻度并标注数值；
// label 置于轴右侧。与 draw_axis_y 共用 inner 的 y_scale()，因此"把某系列映射到
// 次坐标轴"由调用方（chart）通过为右轴单独构造 PlotArea / 替换 y_scale 完成——
// layout 只负责画，不做坐标映射语义。
TERMCHART_API void draw_axis_y_right(Sink& sink, const PlotArea& area,
                                     const std::vector<double>& ticks,
                                     std::string_view label = {},
                                     const Style& style = Style{});

// ── 兼容入口（内部转 CanvasSink，布局逻辑不复制第二份）──────────────────
TERMCHART_API void draw_axis_x(canvas::Canvas& canvas, const PlotArea& area,
                               const std::vector<double>& ticks, std::string_view label = {},
                               const Style& style = Style{});

TERMCHART_API void draw_axis_x(canvas::Canvas& canvas, const PlotArea& area,
                               const std::vector<double>& ticks,
                               const std::vector<std::string>& labels,
                               std::string_view label = {}, const Style& style = Style{});

TERMCHART_API void draw_axis_y(canvas::Canvas& canvas, const PlotArea& area,
                               const std::vector<double>& ticks, std::string_view label = {},
                               const Style& style = Style{});

TERMCHART_API void draw_axis_y_right(canvas::Canvas& canvas, const PlotArea& area,
                                     const std::vector<double>& ticks,
                                     std::string_view label = {},
                                     const Style& style = Style{});

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_AXIS_HPP
