// dependency.hpp —— 依赖图渲染入口（建在 dag 之上）
//
// 约定：边 A→B 表示"A 依赖 B"。布局与 dag 相同（分层 + 依赖方向一致：所有边
// 从左往右跨层推进，被依赖者位于更右的层）。在 dag 的基础上多一件事：
// **依赖环标记**——mark_cycles 打开时，处于环上的边（两端在同一强连通分量，
// 自环也算）被改写 ArrowStyle::Both（两端箭头，⇄ 观感）以提示互相依赖；
// 这是会改写图的绘制状态的操作，关闭选项则不碰任何 ArrowStyle。
// 环判定用"逐边反向 BFS"（边数 * (V+E)），终端图规模下足够；不引入 Tarjan。
#ifndef TERMCHART_GRAPH_DIAGRAMS_DEPENDENCY_HPP
#define TERMCHART_GRAPH_DIAGRAMS_DEPENDENCY_HPP

#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/diagrams/dag.hpp"
#include "termchart/graph/graph.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct DependencyOptions {
    DagOptions dag{};
    bool mark_cycles = true; // 环上的边两端画箭头（会改写 ArrowStyle）
};

// 就地布局并绘制依赖图，返回整图包围盒。有环时同样退化渲染（见 dag.hpp）。
TERMCHART_API Rect render_dependency(Graph& graph, canvas::Canvas& canvas,
                                     const DependencyOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_DEPENDENCY_HPP