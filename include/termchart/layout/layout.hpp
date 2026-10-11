// layout.hpp —— 图表布局公共类型与工具
//
// layout/ 是 stage 3 的几何层：在 canvas（格粒度画布）之上做"轴/网格/图例/标题"
// 的划分与绘制，本身不碰渲染管线（render 层更高，chart 不依赖 render）。所有
// 绘制函数直接作用于 canvas::Canvas，与未来的 GUI 平行栈共用同一套几何。
#ifndef TERMCHART_LAYOUT_LAYOUT_HPP
#define TERMCHART_LAYOUT_LAYOUT_HPP

#include <string>

#include "termchart/export.hpp"

namespace termchart::layout {

// 四边留白（单位：格）。默认给 y 轴刻度标签留出左侧空间、给 x 轴标签留出底部。
struct Margin {
    double top = 1.0;
    double right = 1.0;
    double bottom = 3.0;
    double left = 8.0;
};

enum class Side { Left, Right, Top, Bottom };
enum class HAlign { Left, Center, Right };
enum class VAlign { Top, Middle, Bottom };

// 把刻度值格式化成短标签：大数少小数位，小数多小数位。确定性强，便于测试。
TERMCHART_API std::string format_tick(double value) noexcept;

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_LAYOUT_HPP
