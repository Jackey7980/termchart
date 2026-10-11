// pan.cpp —— 拖拽平移状态机实现（纯几何，无终端/渲染依赖）
#include "termchart/interaction/pan.hpp"

namespace termchart::interaction {

void PanGesture::press(Point at) noexcept {
    dragging_ = true;
    origin_ = at;
    last_ = at;
}

void PanGesture::release() noexcept { dragging_ = false; }

bool PanGesture::dragging() const noexcept { return dragging_; }

std::optional<Point> PanGesture::drag_to(Point at) noexcept {
    if (!dragging_) {
        return std::nullopt;  // 未按下：忽略该次移动，视口保持不变。
    }
    const Point delta{at.x - last_.x, at.y - last_.y};
    last_ = at;
    return delta;
}

bool PanGesture::drag_into(Point at, Viewport& view) noexcept {
    const std::optional<Point> delta = drag_to(at);
    if (!delta.has_value()) {
        return false;
    }
    // 内容跟随指针：指针位移取反后作为视口平移量（指针右移 → 窗口左移）。
    view.pan_by(-delta->x, -delta->y);
    return true;
}

Point PanGesture::origin() const noexcept { return origin_; }

Point PanGesture::last() const noexcept { return last_; }

Point PanGesture::pointer_displacement() const noexcept {
    return Point{last_.x - origin_.x, last_.y - origin_.y};
}

void PanGesture::reset() noexcept {
    dragging_ = false;
    origin_ = Point{};
    last_ = Point{};
}

}  // namespace termchart::interaction
