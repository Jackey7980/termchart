// widget_tree/progress_bar.hpp —— 进度条（value 0..1 → 填充比例）
//
// 被动控件（无交互）：按 value 填充左起比例。常与 Slider 联动（on_change 里
// set_value）。绘制：整条为轨道色，左起 value*width 为填充色，1px 描边。
#ifndef TERMCHART_GUI_WIDGET_TREE_PROGRESS_BAR_HPP
#define TERMCHART_GUI_WIDGET_TREE_PROGRESS_BAR_HPP

#include <algorithm>

#include "termchart_gui/backend_tk/tk_canvas.hpp"
#include "termchart_gui/fwd.hpp"
#include "termchart_gui/widget_tree/widget.hpp"

namespace termchart_gui {

class ProgressBar final : public Widget {
public:
    explicit ProgressBar(std::string id) : Widget(std::move(id)) {}

    [[nodiscard]] double value() const noexcept { return value_; }
    void set_value(double v) noexcept { value_ = std::clamp(v, 0.0, 1.0); }

    void paint(TkCanvas& c) const override {
        const Rect b = global_bounds();
        c.fill_rect(b, Rgb{45, 45, 52});  // 轨道
        if (value_ > 0.0) {
            c.fill_rect(Rect{b.x, b.y, b.w * value_, b.h}, Rgb{90, 190, 240});  // 填充
        }
        c.stroke_rect(b, Rgb{90, 90, 100}, 1.0);
    }

private:
    double value_ = 0.0;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_PROGRESS_BAR_HPP
