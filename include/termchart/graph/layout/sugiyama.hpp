// sugiyama.hpp —— 四步分层布局（Sugiyama 框架）
//
// 在 layout_layered 的"分层 + 坐标分配"之上补上第二步"同层顺序交叉最小化"
// （median / barycenter 启发式），让复杂 DAG 的同层节点不再按登记顺序简单堆叠
// 而交叉凌乱。四步：① 最长路径分层（含破环）② 同层交叉最小化 ③ 坐标分配
// ④ 边路由（由调用方在布局后调用 route，本函数只做几何）。
#ifndef TERMCHART_GRAPH_LAYOUT_SUGIYAMA_HPP
#define TERMCHART_GRAPH_LAYOUT_SUGIYAMA_HPP

#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"

namespace termchart::graph {

// 四步分层布局。iterations 为交叉最小化双向扫描轮数（默认 4，越大越收敛越慢）。
// 返回所有节点外框的并集。
[[nodiscard]] TERMCHART_API Rect layout_sugiyama(Graph& graph,
                                                 const LayoutOptions& opts = {},
                                                 std::uint32_t iterations = 4);

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_LAYOUT_SUGIYAMA_HPP
