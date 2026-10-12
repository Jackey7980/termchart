// social_network.hpp —— 社交网络图（环形布局的节点-连线视图）
//
// 字符化策略：人画成带边框的盒子并居中标名，按注册顺序均分圆环摆位，
// 关系画直线连线（社交关系无向，默认不带箭头）。环形布局在本层内联实现
// （等弦间隔防重叠），绘制复用 diagrams 的共享骨架。
#ifndef TERMCHART_GRAPH_DIAGRAMS_SOCIAL_NETWORK_HPP
#define TERMCHART_GRAPH_DIAGRAMS_SOCIAL_NETWORK_HPP

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct SocialNetworkOptions {
    LayoutOptions layout{};
    double radius = 0.0;  // 0 = 按节点尺寸与间距自动取
    Style node_style{};
    Style edge_style{};
    bool node_border = true;
    bool arrows = false;
};

// 就地环形布局并绘制社交网络图，返回整图包围盒。
TERMCHART_API Rect render_social_network(Graph& graph, canvas::Canvas& canvas,
                                         const SocialNetworkOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_SOCIAL_NETWORK_HPP
