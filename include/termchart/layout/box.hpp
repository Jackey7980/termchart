// box.hpp —— 绘图区（PlotArea）：画布留白 + 数据↔格 比例尺
//
// 一个 PlotArea 把"画布尺寸 + 四边留白 + 数据域"固化成两个 LinearScale：
//   * x_scale_：数据 x → [inner.left, inner.right]
//   * y_scale_：数据 y → [inner.bottom, inner.top]（注意 y 反转，格坐标向下增长）
// 于是 chart 只需 cell_of(x,y) 就能把任意数据点落到正确的格。
#ifndef TERMCHART_LAYOUT_BOX_HPP
#define TERMCHART_LAYOUT_BOX_HPP

#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/layout/layout.hpp"
#include "termchart/scale/linear.hpp"

namespace termchart::layout {

class PlotArea {
public:
    PlotArea(double canvas_w, double canvas_h, Margin margin, double data_xmin,
             double data_xmax, double data_ymin, double data_ymax) noexcept
        : canvas_w_(canvas_w),
          canvas_h_(canvas_h),
          margin_(margin),
          inner_(margin.left, margin.top,
                 canvas_w - margin.left - margin.right,
                 canvas_h - margin.top - margin.bottom),
          x_scale_(data_xmin, data_xmax, inner_.x, inner_.right()),
          y_scale_(data_ymin, data_ymax, inner_.bottom(), inner_.y) {}

    [[nodiscard]] const Rect& inner_rect() const noexcept { return inner_; }
    [[nodiscard]] const scale::LinearScale& x_scale() const noexcept { return x_scale_; }
    [[nodiscard]] const scale::LinearScale& y_scale() const noexcept { return y_scale_; }
    [[nodiscard]] Margin margin() const noexcept { return margin_; }

    // 数据坐标 → 浮点格坐标（交给 canvas 向下取整）。
    [[nodiscard]] Point cell_of(double x, double y) const noexcept {
        return Point{x_scale_.forward(x), y_scale_.forward(y)};
    }

private:
    double canvas_w_;
    double canvas_h_;
    Margin margin_;
    Rect inner_;
    scale::LinearScale x_scale_;
    scale::LinearScale y_scale_;
};

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_BOX_HPP
