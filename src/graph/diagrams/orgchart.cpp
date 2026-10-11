// orgchart.cpp —— 组织架构图实现
#include "termchart/graph/diagrams/orgchart.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/graph/layout/tree_layout.hpp"
#include "termchart/graph/port.hpp"
#include "termchart/graph/route/junction.hpp"
#include "internal.hpp"

namespace termchart::graph {
namespace {

// 树干连线：父节点东侧 → 树干列（= 子节点框左侧前一列）→ 子节点行 → 子节点西侧。
// 同父兄弟的深度相同，因此**天然共用同一列**做树干，不需要额外分配列。
// 父节点东侧锚点按子节点序号均匀扇出（同一个父节点可能有多个子节点）。
void route_trunk(Graph& graph) {
    std::map<NodeId, int> child_count;
    for (const Edge& edge : graph.edges()) {
        ++child_count[edge.from()];
    }

    std::map<NodeId, int> used;
    for (Edge& edge : graph.edges()) {
        const Node* from = graph.find(edge.from());
        const Node* to = graph.find(edge.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }
        const int total = std::max(1, child_count[edge.from()]);
        const int index = used[edge.from()]++;
        // t 落在 (0,1) 开区间内：避免锚点永远落在边框角上。
        const double t = (static_cast<double>(index) + 1.0) /
                         (static_cast<double>(total) + 1.0);
        const Point a = port_anchor(from->box(), PortSide::East, t);
        const Point b = port_anchor(to->box(), PortSide::West);

        // 树干列取"父框右边框"与"子框左边框"的中点，并夹取到两者之间，
        // 保证左右各**至少 1 格**横线：
        //   * 取 child.x - 1 会让分支横线长度为 0，树干与子框边框挤在一起
        //     （渲染成 `├│`、`││` 这种糊在一起的样子）；
        //   * 同父兄弟的 depth 相同 ⇒ 子框 x 相同 ⇒ 仍然共用同一列做树干。
        const double left_bound = from->box().right();      // 父框右边框的右一格
        const double right_bound = to->box().x - 1.0;       // 子框左边框的左一格
        double trunk_x = std::floor((left_bound + right_bound) / 2.0);
        if (right_bound >= left_bound) {
            trunk_x = std::min(std::max(trunk_x, left_bound), right_bound);
        } else {
            trunk_x = right_bound;  // 间距过窄：退化为紧贴子框（仍保证轴对齐）
        }
        edge.set_route({a, Point{trunk_x, a.y}, Point{trunk_x, b.y}, b});
    }
}

// 接点收口：把"横线接上竖线"的直角画成一个 T 形字符（├ ┤ ┬ ┴ ┼ …），
// 而不是两个独立字符拼出的折角。
//
// 关键点：接点**由折线自己算**，不回读画布。回读会依赖绘制顺序（先写的 ─ 会被
// 后续覆盖、判断邻居时看到的是已改写的字符），而折线里 route[1] 就是"从父节点
// 横着进来"的格、route[2] 就是"往子节点横着出去"的格，位置是确定的。
//
// 字符选择本身见 `graph/route/junction.hpp`（规则只有一份；地铁线路图会是第三个
// 消费者，所以不再在本文件里维护一张 switch 表）。
void fixup_junctions(canvas::Canvas& canvas, const Graph& graph) {
    struct Turn {
        bool from_left = false;   // 父节点方向（横线从左边来）
        bool to_right = false;    // 子节点方向（横线朝右边去）
    };
    // 树干列 → (行 → 该行上的接点方向)
    std::map<double, std::map<double, Turn>> turns;
    std::map<double, std::pair<double, double>> extent;  // 树干列的竖直跨度

    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        if (route.size() < 4) {
            continue;
        }
        const Point entry = route[1];  // 父节点侧接点
        const Point leave = route[2];  // 子节点侧接点
        turns[entry.x][entry.y].from_left = true;
        turns[leave.x][leave.y].to_right = true;

        // 跨度必须用 find/emplace 显式判"首次登记"：std::pair 默认构造是 {0,0}，
        // 拿它当"空"会把 0.0 混进跨度里（曾因此把接点字符全算错）。
        const double lo = std::min(entry.y, leave.y);
        const double hi = std::max(entry.y, leave.y);
        const auto found = extent.find(entry.x);
        if (found == extent.end()) {
            extent.emplace(entry.x, std::make_pair(lo, hi));
        } else {
            found->second.first = std::min(found->second.first, lo);
            found->second.second = std::max(found->second.second, hi);
        }
    }

    for (const auto& [x, column] : turns) {
        const auto span = extent[x];
        for (const auto& [y, turn] : column) {
            const bool up = y > span.first;
            const bool down = y < span.second;
            const bool left = turn.from_left;
            const bool right = turn.to_right;
            const char32_t cp = junction_char(up, down, left, right);
            if (cp == U' ') {
                continue;  // 零方向：这一格本来就不是交汇口
            }
            if (x < 0.0 || y < 0.0) {
                continue;
            }
            const auto cx = static_cast<std::uint32_t>(x);
            const auto cy = static_cast<std::uint32_t>(y);
            if (cx >= canvas.width() || cy >= canvas.height()) {
                continue;
            }
            canvas.put(cx, cy, cp, canvas.at(cx, cy).style);
        }
    }
}

}  // namespace

Rect render_orgchart(Graph& graph, canvas::Canvas& canvas,
                     const OrgChartOptions& options) {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    const Rect bounds = layout_tree(graph, options.tree);
    route_trunk(graph);

    detail::DiagramStyle style;
    style.shape = options.shape;
    style.node_style = options.node_style;
    style.edge_style = options.edge_style;
    style.node_border = options.node_border;
    style.arrows = false;      // 组织架构图不画箭头（层级由树干体现）
    style.edge_labels = false;
    style.auto_route = false;  // 折线已写好：树干形态由本文件决定
    detail::draw_diagram(graph, canvas, style);
    fixup_junctions(canvas, graph);
    return bounds;
}

}  // namespace termchart::graph
