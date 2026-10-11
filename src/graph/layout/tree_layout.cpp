// tree_layout.cpp —— 根树布局实现
#include "termchart/graph/layout/tree_layout.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

namespace termchart::graph {

namespace {

Rect union_of_boxes(const Graph& graph) noexcept {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    double min_x = graph.nodes().front().box().x;
    double min_y = graph.nodes().front().box().y;
    double max_x = graph.nodes().front().box().right();
    double max_y = graph.nodes().front().box().bottom();
    for (const Node& n : graph.nodes()) {
        min_x = std::min(min_x, n.box().x);
        min_y = std::min(min_y, n.box().y);
        max_x = std::max(max_x, n.box().right());
        max_y = std::max(max_y, n.box().bottom());
    }
    return Rect{min_x, min_y, max_x - min_x, max_y - min_y};
}

// 建树：BFS 首次访问为准，只沿出边（回边、重复边自动忽略，故有环图也能摆）。
struct TreeShape {
    std::vector<std::vector<std::size_t>> children;
    std::size_t root = 0;
};

TreeShape build_tree(const Graph& graph) {
    const std::size_t count = graph.nodes().size();
    TreeShape shape;
    shape.children.assign(count, {});

    std::unordered_map<NodeId, std::size_t> index;
    index.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        index[graph.nodes()[i].id()] = i;
    }

    std::vector<int> in_degree(count, 0);
    for (const Edge& edge : graph.edges()) {
        const auto it = index.find(edge.to());
        if (it != index.end()) {
            ++in_degree[it->second];
        }
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (in_degree[i] == 0) {
            shape.root = i;
            break;
        }
    }

    std::vector<bool> seen(count, false);
    std::queue<std::size_t> pending;
    pending.push(shape.root);
    seen[shape.root] = true;
    while (!pending.empty()) {
        const std::size_t u = pending.front();
        pending.pop();
        for (const Edge* edge : graph.edges_of(graph.nodes()[u].id())) {
            if (edge->from() != graph.nodes()[u].id()) {
                continue;  // 只沿出边走
            }
            const auto it = index.find(edge->to());
            if (it == index.end() || seen[it->second]) {
                continue;
            }
            seen[it->second] = true;
            shape.children[u].push_back(it->second);
            pending.push(it->second);
        }
    }
    return shape;
}

// 摆放器：先记录每节点的 (x, 行号)，最后统一按侧别做纵向居中并落格。
// side_ 记 0（右侧/单向）或 1（左侧），供 flush 分侧偏移。
class TreePlacer {
public:
    TreePlacer(Graph& graph, const TreeShape& shape, const TreeOptions& opts)
        : graph_(graph), shape_(shape), opts_(opts) {
        for (const Node& node : graph.nodes()) {
            const Rect size = node_box_for(node, opts.layout);
            col_w_ = std::max(col_w_, size.w);
            row_h_ = std::max(row_h_, size.h);
        }
        col_step_ = col_w_ + opts.layout.h_gap;
        row_step_ = row_h_ + opts.row_gap;
        pending_.resize(graph.nodes().size());
        side_.assign(graph.nodes().size(), 0);
    }

    [[nodiscard]] double col_w() const noexcept { return col_w_; }
    [[nodiscard]] double col_step() const noexcept { return col_step_; }
    [[nodiscard]] double row_step() const noexcept { return row_step_; }

    // 递归摆放子树，返回其行中心；next_row 为进出口行游标。
    double place(std::size_t u, int depth, double x_base, double direction, int side,
                 double& next_row) {
        side_[u] = side;
        const std::vector<std::size_t>& kids = shape_.children[u];
        double center_row = next_row;
        if (kids.empty()) {
            ++next_row;
        } else {
            double sum = 0.0;
            for (const std::size_t v : kids) {
                sum += place(v, depth + 1, x_base, direction, side, next_row);
            }
            center_row = sum / static_cast<double>(kids.size());
        }
        double x = x_base + direction * static_cast<double>(depth) * col_step_;
        if (direction < 0.0) {
            x -= col_w_;  // 向左生长时按列右对齐，列内不错位
        }
        pending_[u] = Pending{x, center_row};
        return center_row;
    }

    // 标记该节点已单独摆放（左右分列模式下的根），flush 时跳过它。
    void mark_placed(std::size_t u) noexcept { side_[u] = kPlacedSide; }

    // side < 0 表示写出全部节点（单向模式）。
    void flush(double offset_rows, int side) {
        for (std::size_t i = 0; i < graph_.nodes().size(); ++i) {
            if (side >= 0 && side_[i] != side) {
                continue;
            }
            const Rect size = node_box_for(graph_.nodes()[i], opts_.layout);
            const double y =
                opts_.layout.margin + (pending_[i].row + offset_rows) * row_step_;
            graph_.nodes()[i].set_box(
                Rect{std::round(pending_[i].x), std::round(y), size.w, size.h});
        }
    }

private:
    // 侧别：0 = 右侧/单向，1 = 左侧，2 = 单独摆放（根）。
    static constexpr int kPlacedSide = 2;

    struct Pending {
        double x = 0.0;
        double row = 0.0;
    };

    Graph& graph_;
    const TreeShape& shape_;
    const TreeOptions& opts_;
    std::vector<Pending> pending_;
    std::vector<int> side_;
    double col_w_ = 0.0;
    double row_h_ = 0.0;
    double col_step_ = 0.0;
    double row_step_ = 0.0;
};

// 子树最大深度（start 自身算第 1 层）。
int depth_of(const TreeShape& shape, std::size_t start) {
    int best = 0;
    std::vector<std::pair<std::size_t, int>> stack{{start, 1}};
    while (!stack.empty()) {
        const auto [u, depth] = stack.back();
        stack.pop_back();
        best = std::max(best, depth);
        for (const std::size_t v : shape.children[u]) {
            stack.emplace_back(v, depth + 1);
        }
    }
    return best;
}

}  // namespace

Rect layout_tree(Graph& graph, const TreeOptions& opts) {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    const TreeShape shape = build_tree(graph);
    TreePlacer placer(graph, shape, opts);

    if (!opts.left_right) {
        double next_row = 0.0;
        (void)placer.place(shape.root, 0, opts.layout.margin, 1.0, 0, next_row);
        placer.flush(0.0, -1);
        return union_of_boxes(graph);
    }

    // 左右分列：根的孩子按登记顺序一分为二（偶数向右、奇数向左）。
    std::vector<std::size_t> right_kids;
    std::vector<std::size_t> left_kids;
    const std::vector<std::size_t>& root_kids = shape.children[shape.root];
    for (std::size_t k = 0; k < root_kids.size(); ++k) {
        (k % 2 == 0 ? right_kids : left_kids).push_back(root_kids[k]);
    }

    int left_depth = 0;
    for (const std::size_t u : left_kids) {
        left_depth = std::max(left_depth, depth_of(shape, u));
    }

    // 根列位置：给左侧留出 left_depth 列 + 根自身一列，保证最左节点仍在边距内。
    const double col_step = placer.col_step();
    const double root_x = opts.layout.margin + placer.col_w() +
                          static_cast<double>(left_depth) * col_step;

    double right_rows = 0.0;
    double left_rows = 0.0;
    for (const std::size_t u : right_kids) {
        (void)placer.place(u, 1, root_x, 1.0, 0, right_rows);
    }
    for (const std::size_t u : left_kids) {
        (void)placer.place(u, 1, root_x, -1.0, 1, left_rows);
    }

    // 纵向：两侧各自居中，根落在较大一侧的中点行。
    const double max_rows = std::max({right_rows, left_rows, 1.0});
    const double right_offset = (max_rows - right_rows) / 2.0;
    const double left_offset = (max_rows - left_rows) / 2.0;
    const double root_row = (max_rows - 1.0) / 2.0;

    const Rect root_size = node_box_for(graph.nodes()[shape.root], opts.layout);
    graph.nodes()[shape.root].set_box(
        Rect{std::round(root_x),
             std::round(opts.layout.margin + root_row * placer.row_step()), root_size.w,
             root_size.h});
    placer.mark_placed(shape.root);  // 根已写好，别被 flush 覆盖

    placer.flush(right_offset, 0);
    placer.flush(left_offset, 1);
    return union_of_boxes(graph);
}

}  // namespace termchart::graph
