// reference.hpp —— y 轴零线参考线 + 负区底色（SymLog 等跨零轴的视觉辅助）
//
// 设计：与 PlotArea 解耦——几何（inner 矩形）与映射（Scale）分开传，因此同一个
// 实现既能服务 LinearScale，也能服务 SymLogScale（只要调用方把对应的 y_scale 传
// 进来；PlotArea 当前只内置 LinearScale，SymLog 接线是另一件事）。
//
// 默认行为完全不变：这些函数只在「数据域确实含 0 / 含负数」时才落笔，且需调用方
// 显式调用——不接就不画。零线是横向参考线（数据值 = 0 处），默认加粗（Attr::Bold）；
// 负区底色给数据 < 0 的区域铺一层背景，便于在跨零轴上识别负数区。
#ifndef TERMCHART_LAYOUT_REFERENCE_HPP
#define TERMCHART_LAYOUT_REFERENCE_HPP

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/scale/scale.hpp"

namespace termchart::canvas {
class Canvas;
}
namespace termchart::layout {
class Sink;
}  // namespace termchart::layout

namespace termchart::layout {

// 在 y 方向画一条横向「零线」参考线（数据值 = 0 处）。
// 仅当 0 落在 scale 的数据域内才绘制；线默认加粗（Attr::Bold）以突出。
// 几何范围由 inner 决定（通常传 area.inner_rect()）。
TERMCHART_API void draw_zero_line_y(Sink& sink, const Rect& inner, const scale::Scale& y_scale,
                                    const Style& style = Style{});
TERMCHART_API void draw_zero_line_y(canvas::Canvas& canvas, const Rect& inner,
                                    const scale::Scale& y_scale, const Style& style = Style{});
TERMCHART_API void draw_zero_line_y(Sink& sink, const PlotArea& area, const scale::Scale& y_scale,
                                    const Style& style = Style{});
TERMCHART_API void draw_zero_line_y(canvas::Canvas& canvas, const PlotArea& area,
                                    const scale::Scale& y_scale, const Style& style = Style{});

// 给 y 轴负区（数据值 < 0 的区域）铺一层底色，便于在跨零轴上识别负数区。
// 仅当数据域含负数（domain_min < 0）才绘制。Canvas 专用——Sink 无填充原语，
// 矢量路径（Scene）由调用方用其自身填充能力处理。
TERMCHART_API void shade_negative_y(canvas::Canvas& canvas, const Rect& inner,
                                    const scale::Scale& y_scale, const Style& style = Style{});
TERMCHART_API void shade_negative_y(canvas::Canvas& canvas, const PlotArea& area,
                                    const scale::Scale& y_scale, const Style& style = Style{});

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_REFERENCE_HPP
