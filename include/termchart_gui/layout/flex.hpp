// layout/flex.hpp —— GUI 弹性布局（像素矩形，row/column）
//
// 与终端 layout/ 无关：这里的主轴/交叉轴是像素。算法是经典 flex 的精简版——
// 非 Fill 子项取内容尺寸，Fill 子项按 flex 权重瓜分剩余空间，交叉轴按 cross_align
// 对齐。足以驱动 widget_tree 的容器，且可独立测试。
#ifndef TERMCHART_GUI_LAYOUT_FLEX_HPP
#define TERMCHART_GUI_LAYOUT_FLEX_HPP

#include <algorithm>
#include <vector>

#include "termchart_gui/fwd.hpp"
#include "termchart_gui/layout/constraint.hpp"
#include "termchart_gui/layout/layout.hpp"
#include "termchart_gui/widget_tree/widget.hpp"

namespace termchart_gui {

enum class FlexDirection { Row, Column };
enum class FlexAlign { Start, Center, End, Stretch };

class FlexLayout final : public Layout {
public:
    FlexDirection direction = FlexDirection::Column;
    double gap = 0.0;
    FlexAlign main_align = FlexAlign::Start;    // 主轴整体偏移（剩余空间分配后）
    FlexAlign cross_align = FlexAlign::Stretch;  // 交叉轴子项对齐

    [[nodiscard]] Size measure(const Rect& avail,
                               const std::vector<LayoutItem>& items) const override {
        const bool row = direction == FlexDirection::Row;
        double main = 0.0;
        double cross = 0.0;
        const double gap_total = gap * (items.empty() ? 0.0 : static_cast<double>(items.size() - 1));
        for (const auto& it : items) {
            const double cm = row ? it.content.w : it.content.h;
            const double cc = row ? it.content.h : it.content.w;
            main += cm;
            cross = std::max(cross, cc);
        }
        main += gap_total;
        return row ? Size{main, cross} : Size{cross, main};
    }

    void arrange(const Rect& rect, std::vector<LayoutItem>& items) override {
        const bool row = direction == FlexDirection::Row;
        const double main_extent = row ? rect.w : rect.h;
        const double cross_extent = row ? rect.h : rect.w;
        const std::size_t n = items.size();

        double fixed_total = 0.0;
        double flex_total = 0.0;
        for (const auto& it : items) {
            const bool fill_main = (row ? it.hpolicy : it.vpolicy) == SizePolicy::Fill;
            if (fill_main) {
                flex_total += (it.flex > 0.0 ? it.flex : 1.0);
            } else {
                fixed_total += (row ? it.content.w : it.content.h);
            }
        }
        const double gap_total = gap * static_cast<double>(n > 0 ? n - 1 : 0);
        const double remaining = main_extent - fixed_total - gap_total;
        const double per_flex = (flex_total > 0.0 && remaining > 0.0)
                                   ? remaining / flex_total
                                   : 0.0;

        double used_main = fixed_total + gap_total;
        if (flex_total > 0.0 && remaining > 0.0) {
            used_main += remaining;
        }
        double cursor = 0.0;
        if (main_align == FlexAlign::Center) {
            cursor = (main_extent - used_main) / 2.0;
        } else if (main_align == FlexAlign::End) {
            cursor = main_extent - used_main;
        }
        if (cursor < 0.0) {
            cursor = 0.0;
        }

        for (auto& it : items) {
            const bool fill_main = (row ? it.hpolicy : it.vpolicy) == SizePolicy::Fill;
            const bool stretch_cross = cross_align == FlexAlign::Stretch;
            double m = row ? it.content.w : it.content.h;
            if (fill_main) {
                m = std::max(0.0, per_flex * (it.flex > 0.0 ? it.flex : 1.0));
            }
            double c = row ? it.content.h : it.content.w;
            if (stretch_cross) {
                c = cross_extent;
            }
            double cx = 0.0;
            if (cross_align == FlexAlign::Center) {
                cx = (cross_extent - c) / 2.0;
            } else if (cross_align == FlexAlign::End) {
                cx = cross_extent - c;
            }
            if (cx < 0.0) {
                cx = 0.0;
            }

            const double px = row ? rect.x + cursor : rect.x + cx;
            const double py = row ? rect.y + cx : rect.y + cursor;
            const double pw = row ? m : c;
            const double ph = row ? c : m;
            if (it.widget) {
                it.widget->set_bounds(Rect{px, py, pw, ph});
            }
            cursor += m + gap;
        }
    }
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_LAYOUT_FLEX_HPP
