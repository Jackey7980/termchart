// dock.hpp —— 五区停靠布局（top/bottom/left/right/center）
#ifndef TERMCHART_WIDGET_DOCK_HPP
#define TERMCHART_WIDGET_DOCK_HPP

#include <cstddef>
#include <utility>
#include <vector>

#include "termchart/widget/container.hpp"

namespace termchart::widget {

// 停靠区域。处理顺序固定为 Top→Bottom→Left→Right→Center，从而嵌套出正确的
// 剩余矩形（中心区拿到被四边切剩的部分）。
enum class DockRegion {
    Top,
    Bottom,
    Left,
    Right,
    Center,
};

// 停靠布局：边栏按自身尺寸切走对应长条，中心区占据剩余矩形。典型用法是每区一个
// 子控件（多为布局容器）；同区多个子控件会重叠，由调用方嵌套布局容器避免。
class Dock : public Container {
public:
    explicit Dock(std::string id);

    void add(DockRegion region, Widget& child);
    void add(DockRegion region, Widget* child);

    [[nodiscard]] std::span<const std::pair<DockRegion, Widget*>> docked() const noexcept {
        return docked_;
    }

    void layout() override;

private:
    std::vector<std::pair<DockRegion, Widget*>> docked_;
};

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_DOCK_HPP
