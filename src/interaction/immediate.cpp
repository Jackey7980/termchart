// immediate.cpp —— 即时模式路由实现（无终端/渲染/图表依赖）
#include "termchart/interaction/immediate.hpp"

#include <cmath>

namespace termchart::interaction {

ImmediateContext::ImmediateContext(ImmediateBindings bindings) noexcept
    : bindings_(bindings) {}

void ImmediateContext::feed(const Event& event, Viewport& viewport, BoxSelect& select) {
    if (const auto* mouse = std::get_if<MouseEvent>(&event)) {
        handle_mouse(*mouse, viewport, select);
    }
    // 非鼠标事件（Key/Resize/Focus/Paste）：本层不消费，状态一律不动。
}

Point ImmediateContext::mouse() const noexcept { return mouse_; }

bool ImmediateContext::dragging() const noexcept { return pan_.dragging(); }

const ImmediateBindings& ImmediateContext::bindings() const noexcept { return bindings_; }

void ImmediateContext::handle_mouse(const MouseEvent& ev, Viewport& viewport,
                                    BoxSelect& select) noexcept {
    const Point pos{static_cast<double>(ev.x), static_cast<double>(ev.y)};
    mouse_ = pos;
    const int btn = static_cast<int>(ev.button);

    switch (ev.action) {
        case MouseAction::Press:
            if (ev.button == MouseButton::None) {
                break;  // 无按键语义的按下事件不驱动任何手势。
            }
            pressed_ |= 1u << btn;
            if (btn == bindings_.pan_button) {
                pan_.press(pos);
            }
            if (btn == bindings_.select_button) {
                select.begin(pos);
            }
            break;
        case MouseAction::Release:
            if (ev.button == MouseButton::None) {
                break;
            }
            pressed_ &= ~(1u << btn);
            if (btn == bindings_.pan_button) {
                pan_.release();
            }
            if (btn == bindings_.select_button) {
                select.end();
            }
            break;
        case MouseAction::Move:
            // 只看自己维护的按下集合：Move 事件常不带可靠的按键字段。
            if ((pressed_ & (1u << bindings_.pan_button)) != 0u) {
                pan_.drag_into(pos, viewport);  // 未拖拽时内部 no-op，不动视口。
            }
            if ((pressed_ & (1u << bindings_.select_button)) != 0u) {
                select.update(pos);
            }
            break;
        case MouseAction::ScrollUp:
            zoom_wheel(bindings_.wheel_step, viewport);
            break;
        case MouseAction::ScrollDown:
            zoom_wheel(1.0 / bindings_.wheel_step, viewport);
            break;
    }
}

void ImmediateContext::zoom_wheel(double factor, Viewport& viewport) noexcept {
    // 退化输入：wheel_step 非有限或 <= 0（含 1/step 溢出的情形）——不调用
    // zoom_at，明确不改状态（契约要求，见头文件）。
    if (!(std::isfinite(factor) && factor > 0.0)) {
        return;
    }
    viewport.zoom_at(factor, mouse_);
}

}  // namespace termchart::interaction
