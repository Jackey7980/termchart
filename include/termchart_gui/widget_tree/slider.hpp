// widget_tree/slider.hpp —— 水平滑块（拖拽改值 0..1）
//
// 演示状态机式交互：MousePress 命中 → 进入拖拽并按 x 归一化取值；MouseMove
// 拖拽中跟随；MouseRelease 结束。取值 value = (x - left) / width，钳制 [0,1]，
// 变化时触发 on_change。绘制：居中轨道 + 随值移动的拇指块。
#ifndef TERMCHART_GUI_WIDGET_TREE_SLIDER_HPP
#define TERMCHART_GUI_WIDGET_TREE_SLIDER_HPP

#include <algorithm>
#include <functional>

#include "termchart_gui/backend_tk/tk_canvas.hpp"
#include "termchart_gui/fwd.hpp"
#include "termchart_gui/native_loop/event.hpp"
#include "termchart_gui/widget_tree/widget.hpp"

namespace termchart_gui {

class Slider final : public Widget {
public:
    explicit Slider(std::string id) : Widget(std::move(id)) {}

    [[nodiscard]] double value() const noexcept { return value_; }
    void set_value(double v) noexcept {
        value_ = std::clamp(v, 0.0, 1.0);
    }
    void set_on_change(std::function<void(double)> cb) { on_change_ = std::move(cb); }

    void paint(TkCanvas& c) const override {
        const Rect b = global_bounds();
        const double cy = b.y + b.h * 0.5;
        const double track_h = 6.0;
        c.fill_rect(Rect{b.x, cy - track_h * 0.5, b.w, track_h}, Rgb{58, 58, 64});
        const double thumb_w = 10.0;
        const double thumb_h = b.h * 0.8;
        const double tx = b.x + (b.w - thumb_w) * value_;
        c.fill_rect(Rect{tx, b.y + (b.h - thumb_h) * 0.5, thumb_w, thumb_h},
                    Rgb{120, 184, 236});
    }

    bool on_event(const Event& e) override {
        switch (e.type) {
            case EventType::MousePress: {
                if (!hit_test(Point{e.x, e.y})) {
                    return false;
                }
                dragging_ = true;
                update_from_x(e.x);
                return true;
            }
            case EventType::MouseMove: {
                if (!dragging_) {
                    return false;
                }
                update_from_x(e.x);
                return true;
            }
            case EventType::MouseRelease: {
                if (!dragging_) {
                    return false;
                }
                dragging_ = false;
                return true;
            }
            default:
                return false;
        }
    }

    [[nodiscard]] bool dragging() const noexcept { return dragging_; }

private:
    void update_from_x(double x) {
        const Rect b = global_bounds();
        if (b.w <= 0.0) {
            return;
        }
        const double v = std::clamp((x - b.x) / b.w, 0.0, 1.0);
        if (v != value_) {
            value_ = v;
            if (on_change_) {
                on_change_(value_);
            }
        }
    }

    double value_ = 0.0;
    bool dragging_ = false;
    std::function<void(double)> on_change_;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_SLIDER_HPP
