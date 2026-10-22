// widget_tree/container.hpp —— 容器（持有 Layout，驱动子项几何 + 递归重排）
//
// 容器把自身 bounds 交给 Layout 编排子 Widget 几何，再递归子容器，从而一次
// relayout() 拍平整棵控件树。
#ifndef TERMCHART_GUI_WIDGET_TREE_CONTAINER_HPP
#define TERMCHART_GUI_WIDGET_TREE_CONTAINER_HPP

#include <memory>
#include <vector>

#include "termchart/core/style.hpp"
#include "termchart_gui/fwd.hpp"
#include "termchart_gui/layout/constraint.hpp"
#include "termchart_gui/layout/flex.hpp"
#include "termchart_gui/layout/layout.hpp"
#include "termchart_gui/widget_tree/node.hpp"
#include "termchart_gui/widget_tree/widget.hpp"

namespace termchart_gui {

class Container : public Widget {
public:
    explicit Container(std::string id, Style style = {})
        : Widget(std::move(id), style) {
        auto fl = std::make_unique<FlexLayout>();
        fl->direction = FlexDirection::Column;
        fl->cross_align = FlexAlign::Stretch;
        layout_ = std::move(fl);
    }

    void set_layout(std::unique_ptr<Layout> layout) { layout_ = std::move(layout); }
    [[nodiscard]] Layout* layout() noexcept { return layout_.get(); }
    [[nodiscard]] const Layout* layout() const noexcept { return layout_.get(); }

    // 重排：用 layout_ 把子 Widget 放进自身 bounds，再递归子容器。
    void relayout() {
        if (!layout_) {
            return;
        }
        std::vector<LayoutItem> items;
        items.reserve(children_.size());
        for (const auto& c : children_) {
            if (Widget* w = c->as_widget()) {
                LayoutItem it;
                it.widget = w;
                it.content = Size{w->width(), w->height()};
                it.hpolicy = w->hpolicy();  // 把控件声明的尺寸策略交给布局器
                it.vpolicy = w->vpolicy();
                it.flex = w->flex();
                items.push_back(it);
            }
        }
        // 布局在容器局部坐标（原点=容器左上角）下进行：子项 bounds 相对容器，
        // 由 global_bounds() 累加父链得到全局坐标。
        layout_->arrange(Rect{0.0, 0.0, bounds_.w, bounds_.h}, items);
        for (const auto& c : children_) {
            if (Container* cont = c->as_container()) {
                cont->relayout();
            }
        }
    }

    void paint(TkCanvas& canvas) const override {
        if (!visible_) {
            return;
        }
        Widget::paint(canvas);  // 先画容器自身背景
        for (const auto& c : children_) {
            if (const Widget* w = c->as_widget()) {
                w->paint(canvas);
            }
        }
    }

    [[nodiscard]] bool hit_test(const Point& global) const override {
        if (!visible_ || !enabled_) {
            return false;
        }
        if (!global_bounds().contains(global)) {
            return false;
        }
        for (const auto& c : children_) {
            if (c->hit_test(global)) {
                return true;
            }
        }
        return true;
    }

    [[nodiscard]] Container* as_container() noexcept override { return this; }

private:
    std::unique_ptr<Layout> layout_;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_CONTAINER_HPP
