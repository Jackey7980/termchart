// treemap.cpp —— 树图实现：slice-and-dice 矩形切片（布局自给，不依赖 graph/）
#include "termchart/chart/hierarchical/treemap.hpp"

#include "termchart/layout/title.hpp"

namespace termchart::chart {
namespace {

void slice(const hier::HierNode& node, const Rect& rect, canvas::Canvas& canvas,
           const TreemapOptions& opt) {
    if (rect.w <= 0.0 || rect.h <= 0.0) {
        return;
    }
    if (node.is_leaf()) {
        const Style st = Style{}.set_fg(node.color);
        canvas.fill_rect(rect, kBarChar, st);
        if (opt.show_labels && rect.w >= 2.0 && rect.h >= 1.0 && !node.name.empty()) {
            canvas.put(Point{rect.x, rect.y}, hier::first_codepoint(node.name), st);
        }
        return;
    }
    double total = 0.0;
    for (const hier::HierNode& child : node.children) {
        total += child.value > 0.0 ? child.value : 0.0;
    }
    if (total <= 0.0) {
        return;
    }
    const bool vertical_split = rect.w >= rect.h;  // 沿较长边切，格子尽量偏方
    double offset = vertical_split ? rect.x : rect.y;
    for (const hier::HierNode& child : node.children) {
        const double v = child.value > 0.0 ? child.value : 0.0;
        const double frac = v / total;
        const Rect sub = vertical_split
                             ? Rect{offset, rect.y, rect.w * frac, rect.h}
                             : Rect{rect.x, offset, rect.w, rect.h * frac};
        slice(child, sub, canvas, opt);
        offset += vertical_split ? sub.w : sub.h;
    }
}

}  // namespace

void Treemap::set_root(hier::HierNode root) {
    root_ = std::move(root);
    has_root_ = true;
    hier::aggregate_values(root_);
    hier::assign_colors(root_);
}

void Treemap::draw(DrawContext ctx) {
    if (!has_root_ || root_.value <= 0.0) {
        return;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect area{m.left, m.top,
                    static_cast<double>(ctx.canvas.width()) - m.left - m.right,
                    static_cast<double>(ctx.canvas.height()) - m.top - m.bottom};
    slice(root_, area, ctx.canvas, options);
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, area);
    }
}

}  // namespace termchart::chart
