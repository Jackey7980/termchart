// widget_tree/checkbox.hpp —— 复选框（点击切换 checked + 回调）
//
// 演示"状态型"控件：MousePress 命中即翻转 checked_ 并触发 on_toggle。绘制：左侧
// 方形框（选中时内填绿色块）+ 右侧点阵标签。命中测试复用 Node::hit_test。
#ifndef TERMCHART_GUI_WIDGET_TREE_CHECKBOX_HPP
#define TERMCHART_GUI_WIDGET_TREE_CHECKBOX_HPP

#include <algorithm>
#include <functional>
#include <string>

#include "termchart_gui/backend_tk/tk_canvas.hpp"
#include "termchart_gui/fwd.hpp"
#include "termchart_gui/native_loop/event.hpp"
#include "termchart_gui/widget_tree/widget.hpp"

namespace termchart_gui {

class Checkbox final : public Widget {
public:
    explicit Checkbox(std::string id, std::string label)
        : Widget(std::move(id)), label_(std::move(label)) {}

    [[nodiscard]] bool checked() const noexcept { return checked_; }
    void set_checked(bool c) noexcept { checked_ = c; }
    void set_on_toggle(std::function<void(bool)> cb) { on_toggle_ = std::move(cb); }

    void set_label(std::string l) { label_ = std::move(l); }
    [[nodiscard]] const std::string& label() const noexcept { return label_; }

    void paint(TkCanvas& c) const override {
        const Rect b = global_bounds();
        c.fill_rect(b, Rgb{26, 26, 30});
        const double box = std::min(b.h - 4.0, 14.0);
        const Rect cb{b.x + 2.0, b.y + (b.h - box) * 0.5, box, box};
        c.fill_rect(cb, Rgb{40, 40, 46});
        c.stroke_rect(cb, Rgb{120, 120, 130}, 1.0);
        if (checked_) {
            c.fill_rect(Rect{cb.x + 3.0, cb.y + 3.0, cb.w - 6.0, cb.h - 6.0},
                        Rgb{90, 200, 120});  // 勾选标记
        }
        c.draw_text(cb.x + box + 6.0, b.y + (b.h - 8.0) * 0.5, label_, Rgb{225, 225, 230});
    }

    bool on_event(const Event& e) override {
        if (e.type == EventType::MousePress && hit_test(Point{e.x, e.y})) {
            checked_ = !checked_;
            if (on_toggle_) {
                on_toggle_(checked_);
            }
            return true;
        }
        return false;
    }

private:
    std::string label_;
    bool checked_ = false;
    std::function<void(bool)> on_toggle_;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_CHECKBOX_HPP
