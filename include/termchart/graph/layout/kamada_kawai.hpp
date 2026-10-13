// kamada_kawai.hpp —— KK 全局能量布局（弹簧模型）
//
// 以图的最短路径（跳数）为目标距离做弹簧能量最小化：每轮选能量残差最大的节点
// 对，对其一端做 Newton 迭代解局部能量极小，直到所有残差低于 tolerance 或迭代
// 耗尽。初始摆放固定为圆环（不用随机数），同一张图两次布局结果完全一致。
//
// 退化口径：N=0 返回空矩形；N=1 只落一个节点框；N=2 直接按目标距离水平对放，
// 不进迭代；非连通图按连通分量各自独立布局后从左到右并排摆放（分量间无弹簧
// 约束，最短路径距离不存在）。
#ifndef TERMCHART_GRAPH_LAYOUT_KAMADA_KAWAI_HPP
#define TERMCHART_GRAPH_LAYOUT_KAMADA_KAWAI_HPP

#include <cstdint>

#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"

namespace termchart::graph {

struct KamadaKawaiOptions {
    LayoutOptions layout{};              // 节点尺寸与边距
    double edge_length = 12.0;           // 单位跳数的目标距离（格）
    std::uint32_t max_iterations = 200;  // 主循环（选对 + 移动）上限
    double tolerance = 1e-3;             // 最大残差（相对目标距离）低于此值提前收敛
};

// KK 全局能量布局：就地写入 Node::set_box，返回所有节点外框的并集。
[[nodiscard]] TERMCHART_API Rect layout_kamada_kawai(
    Graph& graph, const KamadaKawaiOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_LAYOUT_KAMADA_KAWAI_HPP
