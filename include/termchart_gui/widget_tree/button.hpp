// widget_tree/button.hpp —— 按钮（按下态 + 点击回调）
//
// 演示带交互的 Widget：on_event 命中测试后置 pressed_ 视觉态并触发 on_click。
// 绘制：底色（按下更深）+ 1px 描边 + 居中点阵标签。命中测试复用 Node::hit_test
// （全局坐标）。on_event 返回 true 表示已消费该事件（阻止继续派发）。
#ifndef TERMCHART_GUI_WIDGET_TREE_BUTTON_HPP
#define TERMCHART_GUI_WIDGET_TREE_BUTTON_HPP

#include <functional>
#include <string>

#include "termchart_gui/backend_tk/tk_canvas.hpp"
#include "termchart_gui/fwd.hpp"
#include "termchart_gui/native_loop/event.hpp"
#include "termchart_gui/widget_tree/widget.hpp"

namespace termchart_gui {

class Button final : public Widget {
public:
    explicit Button(std::string id, std::string label)
        : Widget(std::move(id)), label_(std::move(label)) {}

    void set_label(std::string l) { label_ = std::move(l); }
    [[nodiscard]] const std::string& label() const noexcept { return label_; }

    [[nodiscard]] bool pressed() const noexcept { return pressed_; }
    void set_on_click(std::function<void()> cb) { on_click_ = std::move(cb); }

    void paint(TkCanvas& c) const override {
        const Rect b = global_bounds();
        const Rgb base = pressed_ ? Rgb{74, 74, 84} : Rgb{50, 50, 58};
        c.fill_rect(b, base);
        c.stroke_rect(b, Rgb{124, 124, 136}, 1.0);
        const double tw = static_cast<double>(label_.size()) * kCellW;
        const double tx = b.x + (b.w - tw) * 0.5;
        const double ty = b.y + (b.h - kCellH) * 0.5;
        c.draw_text(tx < b.x ? b.x + 4.0 : tx, ty, label_, Rgb{236, 236, 240});
    }

    bool on_event(const Event& e) override {
        if (e.type == EventType::MousePress) {
            if (hit_test(Point{e.x, e.y})) {
                pressed_ = true;
                if (on_click_) {
                    on_click_();
                }
                return true;
            }
        } else if (e.type == EventType::MouseRelease) {
            const bool was = pressed_;
            pressed_ = false;
            return was;  // 只有在捕获按下后释放才算消费
        }
        return false;
    }

private:
    static constexpr double kCellW = 6.0;  // 与 BitmapCanvas 字格宽一致
    static constexpr double kCellH = 8.0;
    std::string label_;
    bool pressed_ = false;
    std::function<void()> on_click_;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_BUTTON_HPP
