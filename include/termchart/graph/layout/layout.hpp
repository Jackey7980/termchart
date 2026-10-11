// layout.hpp —— 图布局（把节点摆到画布格上，写入 Node::set_box）
//
// 布局只做几何：给定图与选项，为每个节点算出外框。它不画任何东西，也不
// 关心节点形状与连线，因此同一份布局结果可以被字符画、SVG 或 GUI 复用。
#ifndef TERMCHART_GRAPH_LAYOUT_LAYOUT_HPP
#define TERMCHART_GRAPH_LAYOUT_LAYOUT_HPP

#include <cstdint>

#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"

namespace termchart::graph {

struct LayoutOptions {
    double min_width = 10.0;   // 节点外框最小宽（格）
    // 最小高 4：3 行的框除上下边框外只剩 1 个内部行，同侧多条边会被迫挤到
    // 边框行上（观感像从角上进出）。4 行有 2 个内部行可供端口扇出。
    double min_height = 4.0;
    double h_gap = 4.0;        // 水平间距
    double v_gap = 2.0;        // 垂直间距
    double margin = 1.0;       // 画布边距
};

// 单个节点的外框尺寸（原点在 0,0）：宽度按标签显示宽度取 max(最小宽, 标签宽+2)。
[[nodiscard]] TERMCHART_API Rect node_box_for(const Node& node,
                                              const LayoutOptions& opts) noexcept;

// 网格布局：按 cols 列从左到右、从上到下摆放；cols 为 0 时按 1 处理。
// 返回所有节点外框的并集。
[[nodiscard]] TERMCHART_API Rect layout_grid(Graph& graph, std::uint32_t cols,
                                              const LayoutOptions& opts = {});

// 分层布局：按最长路径给节点定层（层即列），层内按登记顺序纵向排列。
// 有向环上的节点无法定层，退到第 0 层（布局仍保证不重叠）。
// 返回所有节点外框的并集。
[[nodiscard]] TERMCHART_API Rect layout_layered(Graph& graph,
                                                 const LayoutOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_LAYOUT_LAYOUT_HPP
