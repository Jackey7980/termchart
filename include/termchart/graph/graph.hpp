// graph.hpp —— 图容器（节点 + 有向边 + 邻接查询）
//
// Graph 是图可视化的语义层：只管"有哪些节点、怎么连"，不管布局与绘制。
// 布局（graph/layout）、路由（graph/route）、形状（graph/shape）与图元
// （graph/diagrams）都在这之上构建，因此同一张 Graph 可以被多种渲染消费。
//
// 增操作的返回类型**刻意不对称**（2026-11-13 复核后定稿，不做破坏性统一）：
//   add_node 不会失败 ⇒ Node&；add_edge 端点可能不存在 ⇒ Edge*（失败 = nullptr）。
// 统一成引用会丢掉失败通道，统一成 Id 则要改动 195 处调用点且无功能收益。
// 真正统一的是使用约定：**返回值仅供即时使用**，两者都可能因后续插入失效；
// 长期持有一律走身份查询 find(NodeId) / find_edge(from, to)。
#ifndef TERMCHART_GRAPH_GRAPH_HPP
#define TERMCHART_GRAPH_GRAPH_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/attributes.hpp"
#include "termchart/graph/edge.hpp"
#include "termchart/graph/node.hpp"

namespace termchart::graph {

class Graph {
public:
    // 新节点 id 从 1 起自增（0 保留为"无效"）。返回引用可能在后续插入后失效。
    Node& add_node(std::string label);

    [[nodiscard]] Node* find(NodeId id) noexcept;
    [[nodiscard]] const Node* find(NodeId id) const noexcept;

    // 删除节点及其全部关联边；不存在返回 false。
    bool remove_node(NodeId id);

    // 端点必须已存在；否则返回 nullptr 且不添加。**返回指针**（而 add_node 返回引用）
    // 是因为加边真的可能失败；需要长期持有请取 `Edge::id()` 再 `find_edge()`。
    Edge* add_edge(NodeId from, NodeId to, std::string label = {});

    // 按端点找回第一条匹配边：与节点的 find(NodeId) 对称（此前只有节点能按 id
    // 找回，边只能靠"当时拿到的指针"——而指针在容器重排后会失效）。
    // 边本身没有 id（from/to 才是它的身份），所以这里按 (from, to) 查。
    [[nodiscard]] Edge* find_edge(NodeId from, NodeId to) noexcept;
    [[nodiscard]] const Edge* find_edge(NodeId from, NodeId to) const noexcept;
    // 删除 from→to 的第一条边；不存在返回 false。
    bool remove_edge(NodeId from, NodeId to);

    [[nodiscard]] const std::vector<Node>& nodes() const noexcept { return nodes_; }
    [[nodiscard]] const std::vector<Edge>& edges() const noexcept { return edges_; }
    // 可变访问：布局写 Node::set_box、路由写 Edge::set_route 时需要。
    [[nodiscard]] std::vector<Node>& nodes() noexcept { return nodes_; }
    [[nodiscard]] std::vector<Edge>& edges() noexcept { return edges_; }
    [[nodiscard]] std::size_t node_count() const noexcept { return nodes_.size(); }
    [[nodiscard]] std::size_t edge_count() const noexcept { return edges_.size(); }

    // 与该节点相邻的边（出边与入边都算），按边的登记顺序。
    [[nodiscard]] TERMCHART_API std::vector<const Edge*> edges_of(NodeId id) const;
    [[nodiscard]] TERMCHART_API std::size_t out_degree(NodeId id) const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t in_degree(NodeId id) const noexcept;

    // 所有节点外框的并集；无节点返回默认空矩形。布局之后才有意义。
    [[nodiscard]] TERMCHART_API Rect bounds() const noexcept;

    // 图级元数据存取口（袋子本体在 private 里）。
    [[nodiscard]] Attributes& attributes() noexcept { return attributes_; }
    [[nodiscard]] const Attributes& attributes() const noexcept { return attributes_; }

    void clear();

private:
    std::vector<Node> nodes_;
    std::vector<Edge> edges_;
    NodeId next_id_ = 1;
    // 图级元数据（不属于任何节点/边）：D2 的全局 style / vars、DOT 的 graph 级
    // 属性、DSL 解析出的图级标注。以前图级属性在 AST → Graph 的转换里无处安放，
    // 只能整段丢弃；给 Graph 一个袋子把它们带过去，渲染侧才能拿到"整图"的语义。
    Attributes attributes_;
};

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_GRAPH_HPP
