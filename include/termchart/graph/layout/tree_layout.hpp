// tree_layout.hpp —— 根树布局（层级 + 兄弟子树行槽）
//
// 做法：从根开始 BFS 建树（首次访问为准，回边与重复边忽略，因此有环图也能摆），
// 后序分配"行槽"——叶子各占一行，父节点的行取子树中心；列由深度决定。
// left_right 模式把根的子树按登记顺序分成两半，一半向右生长、一半镜像向左，
// 根落在中间，用于思维导图。
#ifndef TERMCHART_GRAPH_LAYOUT_TREE_LAYOUT_HPP
#define TERMCHART_GRAPH_LAYOUT_TREE_LAYOUT_HPP

#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"

namespace termchart::graph {

struct TreeOptions {
    LayoutOptions layout{};   // 节点尺寸与边距
    double row_gap = 2.0;     // 行间距
    bool left_right = false;  // true：根居中，子树分列左右（思维导图）
};

// 树布局：就地写入 Node::set_box，返回所有节点外框的并集。
// 根取入度为 0 的第一个节点；全部节点都有入边时取第一个节点。
[[nodiscard]] TERMCHART_API Rect layout_tree(Graph& graph,
                                             const TreeOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_LAYOUT_TREE_LAYOUT_HPP
