// tree_view.cpp —— 树形视图实现：缩进前缀递归（不依赖 graph/）
#include "termchart/chart/hierarchical/tree_view.hpp"

#include <string>

#include "termchart/layout/layout.hpp"
#include "termchart/layout/title.hpp"

namespace termchart::chart {
namespace {

void emit(const hier::HierNode& node, const std::string& prefix, bool last,
          std::size_t& row, canvas::Canvas& canvas, double x, double top,
          const TreeViewOptions& opt) {
    std::string line = prefix;
    line += row == 0 ? "" : (last ? "\u2514\u2500 " : "\u251C\u2500 ");  // └─ / ├─
    line += node.name;
    if (opt.show_values && node.value > 0.0) {
        line += ' ';
        line += layout::format_tick(node.value);
    }
    canvas.draw_text(Point{x, top + static_cast<double>(row)}, line,
                     Style{}.set_fg(node.color));
    ++row;

    std::string child_prefix = prefix;
    if (row > 1) {
        child_prefix += last ? "  " : "\u2502 ";  // │
    }
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        emit(node.children[i], child_prefix, i + 1 == node.children.size(), row, canvas,
             x, top, opt);
    }
}

}  // namespace

void TreeView::set_root(hier::HierNode root) {
    root_ = std::move(root);
    has_root_ = true;
    hier::aggregate_values(root_);
    hier::assign_colors(root_);
}

void TreeView::draw(DrawContext ctx) {
    if (!has_root_) {
        return;
    }
    canvas::Canvas& canvas = ctx.canvas;
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{m.left, m.top,
                     static_cast<double>(canvas.width()) - m.left - m.right,
                     static_cast<double>(canvas.height()) - m.top - m.bottom};
    std::size_t row = 0;
    emit(root_, "", true, row, canvas, inner.x, inner.y, options);
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(canvas, title_, inner);
    }
}

}  // namespace termchart::chart
