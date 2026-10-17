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
    // 层内坐标分配是否做"邻居对齐"（Sugiyama 第三步的朴素版）。默认关。
    // 实测（examples/60_dense_sugiyama，70 节点 / 98 边）：
    //   紧堆（false）：交叉 37、总长 1328 格、拐点 180
    //   朴素对齐（true）：交叉 40、总长 1942 格、拐点 204
    // 朴素版把节点推到"邻居中位数"却不限制层高膨胀，空隙全摊到竖直段上，
    // 除长边折返（2→0）外全指标更差。正确做法需要 Brandes–Köpf 的 block/shift
    // 来约束膨胀，等它落地后再考虑把这步打开。
    bool align_rows = false;
    // align_rows = true 时的层高膨胀上限（相对紧堆高度）。对齐用"图更高"换"边更直"，
    // 但换太多就退回来：超过该比例的层整体回退到紧堆位置，该层的对齐收益放弃、
    // 图高不受影响。0 = 只允许零膨胀（几乎等于不回退），1 = 允许翻倍。
    double align_slack = 0.15;
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
