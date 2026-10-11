// edge.hpp —— 图的有向边（端点 + 标签 + 元数据 + 路由折线）
#ifndef TERMCHART_GRAPH_EDGE_HPP
#define TERMCHART_GRAPH_EDGE_HPP

#include <string>
#include <utility>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/attributes.hpp"
#include "termchart/graph/node.hpp"

namespace termchart::graph {

enum class ArrowStyle {
    None = 0,
    AtTarget,  // 仅终点带箭头
    Both,      // 两端都带箭头
};

class Edge {
public:
    Edge(NodeId from, NodeId to, std::string label = {});

    [[nodiscard]] NodeId from() const noexcept { return from_; }
    [[nodiscard]] NodeId to() const noexcept { return to_; }

    [[nodiscard]] const std::string& label() const noexcept { return label_; }
    void set_label(std::string label) { label_ = std::move(label); }

    [[nodiscard]] Attributes& attributes() noexcept { return attributes_; }
    [[nodiscard]] const Attributes& attributes() const noexcept { return attributes_; }

    [[nodiscard]] ArrowStyle arrow() const noexcept { return arrow_; }
    void set_arrow(ArrowStyle arrow) noexcept { arrow_ = arrow; }

    // 由 graph/route 写入的折线（格坐标，首尾为端点锚点）。
    [[nodiscard]] const std::vector<Point>& route() const noexcept { return route_; }
    void set_route(std::vector<Point> route) { route_ = std::move(route); }

private:
    NodeId from_;
    NodeId to_;
    std::string label_;
    Attributes attributes_;
    ArrowStyle arrow_ = ArrowStyle::AtTarget;
    std::vector<Point> route_;
};

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_EDGE_HPP
