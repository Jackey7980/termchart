// scroll.hpp —— 可滚动容器（带视口偏移，裁剪由调用方负责）
//
// ScrollArea 持有一个"内容坐标空间"，子控件按各自 bounds 留在内容空间里，
// layout() 把每个子控件按其坐标减去 offset 摆到视口（= bounds）坐标系。offset
// 会被夹在 [0, content - viewport] 内（内容小于视口则不滚）。真正的视口裁剪
// （只显示 bounds 覆盖的格）由调用方负责——例如借 Compositor 按偏移合成，或
// 在绘制前对 canvas 做范围判断。
#ifndef TERMCHART_WIDGET_SCROLL_HPP
#define TERMCHART_WIDGET_SCROLL_HPP

#include <utility>

#include "termchart/widget/container.hpp"

namespace termchart::widget {

class ScrollArea : public Container {
public:
    explicit ScrollArea(std::string id);

    void set_content_size(double w, double h) noexcept {
        content_w_ = w;
        content_h_ = h;
        set_dirty();
    }
    [[nodiscard]] std::pair<double, double> content_size() const noexcept {
        return {content_w_, content_h_};
    }

    void set_offset(double x, double y) noexcept;
    [[nodiscard]] std::pair<double, double> offset() const noexcept {
        return {off_x_, off_y_};
    }
    void scroll_by(double dx, double dy) noexcept { set_offset(off_x_ + dx, off_y_ + dy); }

    // 视口 = 容器自身 bounds。
    [[nodiscard]] Rect viewport() const noexcept { return bounds(); }

    void layout() override;

private:
    double content_w_ = 0.0;
    double content_h_ = 0.0;
    double off_x_ = 0.0;
    double off_y_ = 0.0;
};

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_SCROLL_HPP
