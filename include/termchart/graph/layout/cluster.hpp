// cluster.hpp —— 聚类布局（按组内聚、组间分离地摆放节点）
//
// 分组规则：节点带 attributes["cluster"]（字符串）时按属性值分组；没带属性的
// 节点按**无向连通分量**自动归组。每个聚类内部用网格（列数取 ceil(√n)）摆放，
// 整个聚类的包围盒再按网格排布（列数取 ceil(√k)），聚类之间留 cluster_gap。
// 返回所有节点外框的并集；不同聚类的包围盒保证互不重叠。
#ifndef TERMCHART_GRAPH_LAYOUT_CLUSTER_HPP
#define TERMCHART_GRAPH_LAYOUT_CLUSTER_HPP

#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"

namespace termchart::graph {

struct ClusterOptions {
    LayoutOptions layout{};   // 节点尺寸与边距（margin 同时是聚类的起点边距）
    double cluster_gap = 4.0; // 聚类包围盒之间的间距（格）
};

// 聚类布局：就地写入 Node::set_box，返回所有节点外框的并集；空图返回零矩形。
[[nodiscard]] TERMCHART_API Rect layout_cluster(Graph& graph,
                                                const ClusterOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_LAYOUT_CLUSTER_HPP
