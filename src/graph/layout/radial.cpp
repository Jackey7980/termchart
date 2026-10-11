// radial.cpp —— 径向树布局实现
#include "termchart/graph/layout/radial.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <queue>
#include <unordered_map>
#include <vector>

namespace termchart::graph {

namespace {

constexpr double kTwoPi = 6.28318530717958647692;

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

// 根树：children（只沿出边的 BFS 生成树）+ 每个节点的层号。
struct TreeShape {
    std::vector<std::vector<std::size_t>> children;
    std::vector<int> level;
    std::size_t root = 0;
};

// 建树：BFS 首次访问为准，只沿出边（回边、重复边自动忽略，故有环图也能摆）。
// 与 tree_layout 同法，保证两种树布局的根选取规则一致。
TreeShape build_tree(const Graph& graph) {
    const std::size_t count = graph.nodes().size();
    TreeShape shape;
    shape.children.assign(count, {});
    shape.level.assign(count, 0);

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
    // 根取入度为 0 的第一个节点；全有入边（整图是环）时保留下标 0。
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
            shape.level[it->second] = shape.level[u] + 1;
            shape.children[u].push_back(it->second);
            pending.push(it->second);
        }
    }
    // 与根不连通的节点：按登记顺序挂到根下当第 1 层孩子。这样“所有节点都被摆放、
    // 数量与输入一致”，且结果完全确定（不依赖编号/哈希顺序）。
    for (std::size_t i = 0; i < count; ++i) {
        if (!seen[i]) {
            seen[i] = true;
            shape.level[i] = 1;
            shape.children[shape.root].push_back(i);
        }
    }
    return shape;
}

// 递归分配角度：把 [a, b) 扇区的中心给 u，再把扇区按子节点数切给每个孩子。
// min_angle 给相邻子扇区留出角间隙；扇区不够放间隙时退回纯均分（gap = 0）。
void assign_angles(const TreeShape& shape, std::size_t u, double a, double b,
                   double min_angle, std::vector<double>& angle) {
    angle[u] = 0.5 * (a + b);
    const std::vector<std::size_t>& kids = shape.children[u];
    const std::size_t k = kids.size();
    if (k == 0) {
        return;
    }
    const double width = b - a;
    double gap = (k > 1) ? min_angle : 0.0;
    double usable = width - static_cast<double>(k - 1) * gap;
    if (usable <= 0.0) {
        // 扇区容不下 k 个间隙：放弃间隙，退回均分，保证每个孩子仍有非零扇区。
        gap = 0.0;
        usable = width;
    }
    const double sub = usable / static_cast<double>(k);
    for (std::size_t i = 0; i < k; ++i) {
        const double start = a + static_cast<double>(i) * (sub + gap);
        assign_angles(shape, kids[i], start, start + sub, min_angle, angle);
    }
}

}  // namespace

Rect layout_radial(Graph& graph, const RadialOptions& opts) {
    const std::size_t count = graph.nodes().size();
    if (count == 0) {
        return Rect{};
    }

    std::vector<Rect> size(count);
    for (std::size_t i = 0; i < count; ++i) {
        size[i] = node_box_for(graph.nodes()[i], opts.layout);
    }

    const TreeShape shape = build_tree(graph);

    std::vector<double> angle(count, 0.0);
    assign_angles(shape, shape.root, opts.start_angle, opts.start_angle + kTwoPi,
                  std::max(0.0, opts.min_angle), angle);

    // 极坐标 → 格坐标。aspect 防止正圆在非方格上显扁（口径同 canvas/transform.hpp）。
    const double aspect = std::max(0.01, opts.aspect);
    const double step = std::max(0.0, opts.ring_step);
    const double inner = std::max(0.0, opts.ring_start);

    std::vector<double> px(count, 0.0);
    std::vector<double> py(count, 0.0);
    for (std::size_t i = 0; i < count; ++i) {
        const double r = inner + static_cast<double>(shape.level[i]) * step;
        px[i] = r * std::cos(angle[i]);
        py[i] = r * std::sin(angle[i]) / aspect;
    }

    // 落格：与 force.cpp 同法——先取“中心减半框”的最小值，再把相对位置取整后加边距，
    // 保证最靠左/上的框恰好贴到 margin，且所有框都落在返回 Rect 内。
    double min_x = px[0] - size[0].w / 2.0;
    double min_y = py[0] - size[0].h / 2.0;
    for (std::size_t i = 0; i < count; ++i) {
        min_x = std::min(min_x, px[i] - size[i].w / 2.0);
        min_y = std::min(min_y, py[i] - size[i].h / 2.0);
    }
    for (std::size_t i = 0; i < count; ++i) {
        const double x =
            std::round(px[i] - size[i].w / 2.0 - min_x) + opts.layout.margin;
        const double y =
            std::round(py[i] - size[i].h / 2.0 - min_y) + opts.layout.margin;
        graph.nodes()[i].set_box(Rect{x, y, size[i].w, size[i].h});
    }
    return union_of_boxes(graph);
}

}  // namespace termchart::graph
