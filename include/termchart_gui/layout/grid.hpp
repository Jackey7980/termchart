// layout/grid.hpp —— GUI 等距网格布局（像素矩形）
//
// 平行栈的网格：固定列数，子项按行优先铺进等分的单元并拉伸填满单元。不处理
// row/col span（平行栈脚手架阶段够用；需要 span 时再扩展）。
#ifndef TERMCHART_GUI_LAYOUT_GRID_HPP
#define TERMCHART_GUI_LAYOUT_GRID_HPP

#include <algorithm>
#include <cmath>
#include <vector>

#include "termchart_gui/fwd.hpp"
#include "termchart_gui/layout/constraint.hpp"
#include "termchart_gui/layout/layout.hpp"
#include "termchart_gui/widget_tree/widget.hpp"

namespace termchart_gui {

class GridLayout final : public Layout {
public:
    std::size_t columns = 1;
    double hgap = 0.0;
    double vgap = 0.0;

    [[nodiscard]] Size measure(const Rect& avail,
                               const std::vector<LayoutItem>& items) const override {
        (void)items;
        // 网格默认填满可用区域（平行栈脚手架：由容器 bounds 决定单元尺寸）。
        return Size{avail.w, avail.h};
    }

    void arrange(const Rect& rect, std::vector<LayoutItem>& items) override {
        if (columns == 0) {
            columns = 1;
        }
        const std::size_t n = items.size();
        const std::size_t rows =
            (n + columns - 1) / std::max<std::size_t>(columns, 1);
        const double cell_w =
            (rect.w - hgap * static_cast<double>(columns - 1)) / static_cast<double>(columns);
        const double cell_h =
            (rect.h - vgap * static_cast<double>(rows - 1)) /
            static_cast<double>(std::max<std::size_t>(rows, 1));
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t col = i % columns;
            const std::size_t row = i / columns;
            const double x = rect.x + static_cast<double>(col) * (cell_w + hgap);
            const double y = rect.y + static_cast<double>(row) * (cell_h + vgap);
            if (items[i].widget) {
                items[i].widget->set_bounds(Rect{x, y, cell_w, cell_h});
            }
        }
    }
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_LAYOUT_GRID_HPP
