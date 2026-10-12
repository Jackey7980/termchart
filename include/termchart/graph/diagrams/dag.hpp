// dag.hpp —— 有向无环图渲染入口
//
// 入口先做环检测（Kahn 拓扑排序，见 is_acyclic），渲染策略：
//   * 无环：分层布局（层 = 拓扑深度，所有边一律从左往右跨层）+ 正交路由。
//   * 有环：**不报错、不丢边**——退化为普通分层布局（layout_layered 自带破环，
//     环上节点挂到已定层前驱的下一层），环的回边照常画出并带箭头。调用方可先
//     用 is_acyclic() 自行决定是否提示用户。
#ifndef TERMCHART_GRAPH_DIAGRAMS_DAG_HPP
#define TERMCHART_GRAPH_DIAGRAMS_DAG_HPP

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"
#include "termchart/graph/route/route.hpp"
#include "termchart/graph/shape/shape.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct DagOptions {
    LayoutOptions layout{};
    RouteOptions route{};
    ShapeKind shape = ShapeKind::Box;
    Style node_style{};
    Style edge_style{};
    bool arrows = true;
    bool edge_labels = false;
};

// 环检测：Kahn 拓扑排序能覆盖全部节点即无环（自环也算环）。
[[nodiscard]] TERMCHART_API bool is_acyclic(const Graph& graph);

// 就地布局并绘制 DAG，返回整图包围盒。有环时按上面的退化策略渲染。
TERMCHART_API Rect render_dag(Graph& graph, canvas::Canvas& canvas,
                              const DagOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_DAG_HPP