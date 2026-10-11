// circular.hpp —— 圆周布局（所有节点均匀分布在一个圆上）
//
// 与 radial 的“按父子关系分环”不同，圆周布局不看边，只按**节点登记顺序**把
// 全部节点均匀摆在一个圆上：第 i 个节点的角度为 θ_i = start_angle + 2π·i/n。
// 它适合“没有层次、只想看一圈并列节点”的场景（如维度环、雷达式清单）。
//
// 半径由节点数与节点尺寸推出，保证相邻节点框不重叠：
//   相邻中心在格坐标系里的最小距离为 d_min = 2·(R/aspect)·sin(π/n)
//   （y 方向被压缩了 1/aspect，最紧的方向在 x 轴附近，故分母带 aspect）。
// 以节点框的外接圆半径 ρ_i = √(w²+h²)/2 作为“两框不重叠”的**充分**判据：
//   d_min ≥ ρ_i + ρ_j + gap  ⇒  外接圆互不相交  ⇒  矩形互不相交。
// 于是 R = aspect·(max 相邻 ρ_i+ρ_j + gap) / (2·sin(π/n))，再用 min_radius 兜底。
//   · 用外接圆是刻意取保守（“推一个足够的半径”）：它忽略矩形朝向，绝不会算小；
//   · gap 还额外吸收落格取整带来的约 ±1 格误差，默认 2 足够。
// n = 1 时半径取 0（单节点落在圆心）；n ≥ 2 时半径才由上式与 min_radius 共同决定。
#ifndef TERMCHART_GRAPH_LAYOUT_CIRCULAR_HPP
#define TERMCHART_GRAPH_LAYOUT_CIRCULAR_HPP

#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"

namespace termchart::graph {

struct CircularOptions {
    LayoutOptions layout{};       // 节点尺寸与边距
    double gap = 2.0;             // 相邻节点框之间的期望间距（格），并吸收取整误差
    double min_radius = 8.0;      // 半径兜底下限（n ≥ 2 时生效）
    double aspect = 2.0;          // 字符格宽高比（宽:高）；口径同 canvas/transform.hpp
    // 第一个节点的角度（弧度），默认 -π/2，即第一个节点在正上方，顺时针排开。
    double start_angle = -1.57079632679489661923;
};

// 圆周布局：就地写入 Node::set_box，返回所有节点外框的并集；空图返回零矩形。
[[nodiscard]] TERMCHART_API Rect layout_circular(Graph& graph,
                                                 const CircularOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_LAYOUT_CIRCULAR_HPP
