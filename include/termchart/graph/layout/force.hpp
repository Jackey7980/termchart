// force.hpp —— 力导向布局（Fruchterman-Reingold 简化版）
//
// 两条力：节点两两斥力 k^2/d，边方向弹簧引力 d^2/k。每轮按"温度"限制单步
// 位移并逐步降温，收敛后取整落格。
//
// 确定性：初始摆放固定为圆环（不用随机数），迭代与浮点运算顺序固定，因此同一
// 张图两次布局结果完全一致——这样布局才能进 golden 与回归测试。
#ifndef TERMCHART_GRAPH_LAYOUT_FORCE_HPP
#define TERMCHART_GRAPH_LAYOUT_FORCE_HPP

#include <cstdint>

#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"

namespace termchart::graph {

struct ForceOptions {
    LayoutOptions layout{};          // 节点尺寸与边距
    double ideal_edge_length = 12.0; // 理想边长 k
    std::uint32_t iterations = 240;  // 迭代轮数
    double cooling = 0.95;           // 每轮温度衰减
    double min_step = 0.01;          // 温度低于此值提前收敛
    double gravity = 0.05;           // 朝原点的弱引力（收拢无连边分量，防漂散）
};

// 力导向布局：就地写入 Node::set_box，返回所有节点外框的并集。
[[nodiscard]] TERMCHART_API Rect layout_force(Graph& graph,
                                              const ForceOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_LAYOUT_FORCE_HPP
