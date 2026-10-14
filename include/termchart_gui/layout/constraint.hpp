// layout/constraint.hpp —— 布局约束与可布局子项
//
// 平行栈的 layout 层重写（不复用终端 layout/，理由见 docs/design/gui_stack.md）：
// 终端的布局常数是 1:2 字符格比，GUI 用的是像素矩形。这里是 GUI 自己的几何层
// 原语，纯数据、可测试。
#ifndef TERMCHART_GUI_LAYOUT_CONSTRAINT_HPP
#define TERMCHART_GUI_LAYOUT_CONSTRAINT_HPP

#include <cstddef>
#include <vector>

#include "termchart_gui/fwd.hpp"

namespace termchart_gui {

struct Size {
    double w = 0.0;
    double h = 0.0;
};

constexpr double kInfSize = 1e9;

enum class SizePolicy {
    Fixed,        // 取内容自然尺寸，不伸缩
    MinContent,   // 至少内容尺寸，可放大到 Fill
    MaxContent,   // 至多内容尺寸，可缩小
    Fill,         // 在剩余空间内按 flex 权重伸缩
};

// 一个可布局子项：指向控件 + 自身尺寸意图 + 自然尺寸（measure 阶段填）。
struct LayoutItem {
    Widget* widget = nullptr;
    SizePolicy hpolicy = SizePolicy::Fill;
    SizePolicy vpolicy = SizePolicy::Fill;
    double flex = 1.0;  // Fill 时的弹性权重（<=0 视为 0）
    double min_w = 0.0;
    double max_w = kInfSize;
    double min_h = 0.0;
    double max_h = kInfSize;
    Size content{};  // 子项的自然尺寸
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_LAYOUT_CONSTRAINT_HPP
