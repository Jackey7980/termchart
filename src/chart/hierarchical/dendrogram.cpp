// dendrogram.cpp —— 树状图实现：递归布局 + U 形连接线（不依赖 graph/）
#include "termchart/chart/hierarchical/dendrogram.hpp"

#include <cstddef>
#include <vector>

#include "termchart/layout/title.hpp"

namespace termchart::chart {
namespace {

// 预序展平的节点几何：children 下标恒大于父节点下标。
struct NodeGeom {
    double x = 0.0;
    double row = 0.0;
    double child_x0 = 0.0;  // 内部节点：子节点 x 范围（横杠画在这里）
    double child_x1 = 0.0;
    std::size_t first_child = 0;  // 预序下标区间 [first_child, last_child]
    std::size_t last_child = 0;
    bool leaf = false;
    std::size_t depth = 0;
};

struct Walker {
    const hier::HierNode* tree = nullptr;
    std::vector<NodeGeom> nodes;
    std::size_t leaf_count = 0;
    std::size_t leaf_idx = 0;
    std::size_t max_depth = 0;
    Rect inner{};

    void measure(const hier::HierNode& node, std::size_t depth) {
        max_depth = std::max(max_depth, depth);
        if (node.is_leaf()) {
            ++leaf_count;
            return;
        }
        for (const hier::HierNode& child : node.children) {
            measure(child, depth + 1);
        }
    }

    double row_of(std::size_t depth) const {
        if (max_depth == 0) {
            return inner.y;
        }
        return inner.y + static_cast<double>(depth) * (inner.h - 1.0) /
                             static_cast<double>(max_depth);
    }

    double x_of_leaf() const {
        if (leaf_count <= 1) {
            return inner.x + inner.w / 2.0;
        }
        return inner.x + inner.w * static_cast<double>(leaf_idx) /
                             static_cast<double>(leaf_count - 1);
    }

    std::size_t walk(const hier::HierNode& node, std::size_t depth) {
        const std::size_t idx = nodes.size();
        nodes.push_back(NodeGeom{});
        if (node.is_leaf()) {
            const double x = x_of_leaf();
            ++leaf_idx;
            nodes[idx] = NodeGeom{x, row_of(depth), x, x, true, depth};
            return idx;
        }
        double x0 = 0.0;
        double x1 = 0.0;
        const std::size_t first_child = nodes.size();
        std::size_t last_child = first_child;
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            last_child = walk(node.children[i], depth + 1);
            if (i == 0) {
                x0 = nodes[last_child].x;
            }
            x1 = nodes[last_child].x;
        }
        nodes[idx] = NodeGeom{(x0 + x1) / 2.0, row_of(depth), x0, x1, first_child,
                              last_child, false, depth};
        return idx;
    }
};

}  // namespace

void Dendrogram::set_root(hier::HierNode root) {
    root_ = std::move(root);
    has_root_ = true;
    hier::aggregate_values(root_);
    hier::assign_colors(root_);
}

void Dendrogram::draw(DrawContext ctx) {
    if (!has_root_) {
        return;
    }
    canvas::Canvas& canvas = ctx.canvas;
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{m.left, m.top,
                     static_cast<double>(canvas.width()) - m.left - m.right,
                     static_cast<double>(canvas.height()) - m.top - m.bottom};
    if (inner.w < 2.0 || inner.h < 2.0) {
        return;
    }

    Walker walker;
    walker.measure(root_, 0);
    walker.walk(root_, 0);

    // 预序下子节点下标 > 父节点，倒序即"先叶后根"，连接线自底向上画。
    for (std::size_t i = walker.nodes.size(); i-- > 0;) {
        const NodeGeom& node = walker.nodes[i];
        if (node.leaf) {
            if (options.show_leaves) {
                canvas.put(Point{node.x, node.row}, kMarkerChar,
                           Style{}.set_fg(hier::hier_palette(0)));
            }
            continue;
        }
        const Style st = Style{}.set_fg(hier::hier_palette(0));
        // 横杠：在父节点行连接最左/最右子节点的 x。
        canvas.draw_line(Point{node.child_x0, node.row},
                         Point{node.child_x1, node.row}, U'\u2500', st);  // ─
        // 每个子节点从自己的行向上连垂线到父横杠行。
        for (std::size_t c = node.first_child; c <= node.last_child; ++c) {
            const NodeGeom& child = walker.nodes[c];
            canvas.draw_line(Point{child.x, node.row}, Point{child.x, child.row},
                             U'\u2502', st);  // │
        }
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(canvas, title_, inner);
    }
}

}  // namespace termchart::chart
