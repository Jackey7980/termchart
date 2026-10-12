// petri_net.hpp —— Petri 网（库所 / 变迁 / 有向弧 + token）
//
// 字符化策略：库所画椭圆、token 画 ●（超过上限改画数字），变迁画窄高矩形，
// 弧走正交折线带箭头；节点按弧可达深度分列（库所偶数列、变迁奇数列），
// 名称画在形状下方。
#ifndef TERMCHART_GRAPH_DIAGRAMS_PETRI_NET_HPP
#define TERMCHART_GRAPH_DIAGRAMS_PETRI_NET_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/layout/layout.hpp"
#include "termchart/graph/route/route.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct PetriNet {
    struct Place {
        std::string name;
        std::uint32_t tokens = 0;
    };
    struct Transition {
        std::string name;
    };
    // 弧端点：(kind, index) 引用库所或变迁。
    struct Endpoint {
        bool is_place = true;
        std::size_t index = 0;
    };
    struct Arc {
        Endpoint from;
        Endpoint to;
    };

    static constexpr Endpoint place(std::size_t index) noexcept {
        return Endpoint{true, index};
    }
    static constexpr Endpoint transition(std::size_t index) noexcept {
        return Endpoint{false, index};
    }

    std::vector<Place> places;
    std::vector<Transition> transitions;
    std::vector<Arc> arcs;
};

struct PetriNetOptions {
    LayoutOptions layout{};  // margin/h_gap/v_gap 生效；节点尺寸按图种固定
    RouteOptions route{};
    Style place_style{};
    Style transition_style{};
    Style arc_style{};
    std::uint32_t max_token_dots = 5;  // token 数超过上限改画数字
};

// 就地布局并绘制 Petri 网，返回整图包围盒。弧端点越界时该弧被跳过。
TERMCHART_API Rect render_petri_net(const PetriNet& net, canvas::Canvas& canvas,
                                    const PetriNetOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_PETRI_NET_HPP
