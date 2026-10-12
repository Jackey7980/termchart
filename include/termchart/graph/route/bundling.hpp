// bundling.hpp —— 边捆绑（共享目标节点的边在末段并束）
//
// 逐边独立路由时，多条边各占一条走廊涌入目标节点，观感杂乱。边捆绑把
// **共享同一目标节点**的边分成一组：组内先在目标锚点外侧 bundle_gap 格处定一个
// 公共"并束点"，每条边先以正交 Z 形走到并束点，再共享从并束点到锚点的**末段**
// 进框。整组只占用目标的一个锚点与一条末段走廊。
//
// 并束方向：从目标中心指向组内各源点中心的质心，取主轴侧作为进入侧；
// 并束点必须落在所有节点框之外，否则逐步加大距离重试，仍不行则该组放弃捆绑，
// 逐边按普通正交路由。自环边不参与捆绑（锚点重合，没有"末段"可言）。
#ifndef TERMCHART_GRAPH_ROUTE_BUNDLING_HPP
#define TERMCHART_GRAPH_ROUTE_BUNDLING_HPP

#include <cstddef>

#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/route/route.hpp"
#include "termchart/graph/wiregrid.hpp"

namespace termchart::graph {

struct BundlingOptions {
    RouteOptions route{};    // 组内逐边走到并束点的绕行上限
    double bundle_gap = 3.0; // 并束点离目标锚点的距离（格）
};

// 为所有边计算折线并写入 Edge::set_route（首尾为节点边框锚点）。
// grid 非空时在其上避让并占用；为空时自建覆盖全图并留绕行余量的栅格。
// 返回写入折线的边数。
[[nodiscard]] TERMCHART_API std::size_t route_bundled(Graph& graph,
                                                      const BundlingOptions& opts = {},
                                                      WireGrid* grid = nullptr);

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_ROUTE_BUNDLING_HPP
