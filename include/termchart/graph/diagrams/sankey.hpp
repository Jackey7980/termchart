// sankey.hpp —— 桑基图（节点 + 流，按 value 定横条高度、按阶段分层）
//
// 字符化策略：节点画 █ 实心竖条（高 ∝ 流量合计），流画 ░ 丝带（源条右缘到
// 目标条左缘，上下沿线性过渡）；阶段按流的可达深度分列，可按内置调色板
// 着色（节点取调色板色，丝带继承源节点色）。
#ifndef TERMCHART_GRAPH_DIAGRAMS_SANKEY_HPP
#define TERMCHART_GRAPH_DIAGRAMS_SANKEY_HPP

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

struct SankeyNode {
    std::string name;
};

struct SankeyFlow {
    std::size_t source = 0;
    std::size_t target = 0;
    double value = 0.0;
};

struct SankeyOptions {
    double margin = 1.0;
    double bar_width = 2.0;  // 节点横条宽（格）
    double h_gap = 5.0;      // 列间距（丝带宽度下限）
    double v_gap = 1.0;      // 同列相邻条间隙
    Style node_style{};
    Style flow_style{};
    bool colorize = true;  // 节点按内置 6 色调色板着色，丝带继承源节点色
};

// 就地布局并绘制桑基图，返回整图包围盒。端点越界或 value <= 0 的流被跳过。
TERMCHART_API Rect render_sankey(const std::vector<SankeyNode>& nodes,
                                 const std::vector<SankeyFlow>& flows,
                                 canvas::Canvas& canvas,
                                 const SankeyOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_SANKEY_HPP
