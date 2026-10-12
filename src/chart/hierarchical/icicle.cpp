// icicle.cpp —— 冰柱图实现：逐层分段的递归布局（不依赖 graph/）
#include "termchart/chart/hierarchical/icicle.hpp"

#include "termchart/layout/title.hpp"

namespace termchart::chart {
namespace {

void layout_row(const hier::HierNode& node, double x0, double x1, std::size_t depth,
                double top, canvas::Canvas& canvas, const IcicleOptions& opt) {
    if (x1 - x0 < 1.0) {
        return;
    }
    const Rect seg{x0, top + static_cast<double>(depth), x1 - x0, 1.0};
    const Style st = Style{}.set_fg(node.color);
    canvas.fill_rect(seg, kBarChar, st);
    if (opt.show_labels && seg.w >= 2.0 && !node.name.empty()) {
        canvas.put(Point{seg.x, seg.y}, hier::first_codepoint(node.name), st);
    }
    double total = 0.0;
    for (const hier::HierNode& child : node.children) {
        total += child.value > 0.0 ? child.value : 0.0;
    }
    if (total <= 0.0) {
        return;
    }
    double offset = x0;
    for (const hier::HierNode& child : node.children) {
        const double v = child.value > 0.0 ? child.value : 0.0;
        const double w = (x1 - x0) * (v / total);
        layout_row(child, offset, offset + w, depth + 1, top, canvas, opt);
        offset += w;
    }
}

}  // namespace

void Icicle::set_root(hier::HierNode root) {
    root_ = std::move(root);
    has_root_ = true;
    hier::aggregate_values(root_);
    hier::assign_colors(root_);
}

void Icicle::draw(DrawContext ctx) {
    if (!has_root_ || root_.value <= 0.0) {
        return;
    }
    canvas::Canvas& canvas = ctx.canvas;
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{m.left, m.top,
                     static_cast<double>(canvas.width()) - m.left - m.right,
                     static_cast<double>(canvas.height()) - m.top - m.bottom};
    if (inner.w < 2.0 || inner.h < 1.0) {
        return;
    }
    layout_row(root_, inner.x, inner.right(), 0, inner.y, canvas, options);
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(canvas, title_, inner);
    }
}

}  // namespace termchart::chart
