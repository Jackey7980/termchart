// arc_diagram.hpp —— 弧线图（节点一排 + 跨度定高的弧）
//
// 字符化策略：节点按登记顺序沿底行一字排开（标签居中于各自的列），连接画成
// 底行上方的二次贝塞尔弧——跨度（两端列距）越大弧顶越高，高度封顶防出界；
// 弧体逐格按走向取 ─ │ ╱ ╲。同一节点的自连接画成小拱。
#ifndef TERMCHART_GRAPH_DIAGRAMS_ARC_DIAGRAM_HPP
#define TERMCHART_GRAPH_DIAGRAMS_ARC_DIAGRAM_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct ArcNode {
    std::string label;
};

// 一条弧：from/to 是 nodes 的下标；跨度越大弧越高。
struct ArcLink {
    std::size_t from = 0;
    std::size_t to = 0;
};

struct ArcDiagramOptions {
    double h_gap = 3.0;   // 相邻节点之间的空隙（格）
    int max_height = 6;   // 弧顶最大高度（格，自节点行向上计）
    Style label_style{};
    Style arc_style{};
};

// 就地绘制；返回标签与弧的包围盒（节点为空返回零矩形）。下标越界的弧跳过。
[[nodiscard]] TERMCHART_API Rect render_arc_diagram(const std::vector<ArcNode>& nodes,
                                                    const std::vector<ArcLink>& links,
                                                    canvas::Canvas& canvas,
                                                    const ArcDiagramOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_ARC_DIAGRAM_HPP
