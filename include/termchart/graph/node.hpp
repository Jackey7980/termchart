// node.hpp —— 图节点（标识 + 标签 + 元数据 + 布局外框）
//
// Node 只持有"语义"与"布局结果"两类信息：语义（id/label/attributes）由调用方
// 填写，外框（box）由 graph/layout 写入，供 route/ 与渲染消费。节点不持有任何
// 绘制状态，因此同一张图可以用字符画、SVG 或 GUI 三种方式渲染。
#ifndef TERMCHART_GRAPH_NODE_HPP
#define TERMCHART_GRAPH_NODE_HPP

#include <cstdint>
#include <string>
#include <utility>

#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/attributes.hpp"

namespace termchart::graph {

using NodeId = std::uint32_t;

// 0 保留为"无效/无节点"，便于把 0 当作默认值。
inline constexpr NodeId kInvalidNode = 0;

class Node {
public:
    Node(NodeId id, std::string label);

    [[nodiscard]] NodeId id() const noexcept { return id_; }

    [[nodiscard]] const std::string& label() const noexcept { return label_; }
    void set_label(std::string label) { label_ = std::move(label); }

    [[nodiscard]] Attributes& attributes() noexcept { return attributes_; }
    [[nodiscard]] const Attributes& attributes() const noexcept { return attributes_; }

    // 布局结果：节点在画布上的外框（格坐标）。未布局时为默认空矩形。
    [[nodiscard]] const Rect& box() const noexcept { return box_; }
    void set_box(Rect box) noexcept { box_ = box; }

private:
    NodeId id_;
    std::string label_;
    Attributes attributes_;
    Rect box_{};
};

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_NODE_HPP
