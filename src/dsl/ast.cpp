#include "termchart/dsl/ast.hpp"

#include <string>
#include <unordered_map>

#include "termchart/graph/edge.hpp"
#include "termchart/graph/graph.hpp"

namespace termchart::dsl {

// ── 节点索引 ────────────────────────────────────────────────────────────────
//
// 历史实现是**线性扫描** `nodes`：find_node O(N)、ensure_node O(N)，
// 于是"建 N 个节点"退化成 O(N²)；to_graph 的边端点查表同样是 O(N·E)。
// 现在用一张 key → 下标 的哈希索引把这三处降到 O(1) / O(N)。
//
// 三条不变式（改动前请先读完）：
//   1. **nodes 仍是唯一真源**：下标顺序 == 输出顺序，所以绝不能把 nodes 换成
//      unordered_map 容器（会打乱输出顺序）。索引只是加速器。
//   2. **索引可失效且必须能自愈**：外部允许绕过 ensure_node 直接改 nodes
//      （例如 mermaid_flow 直接 push_back），因此查询前要比对 size；
//      不一致就退回线性扫描（冷路径，且不分配，保持 find_node 的 noexcept 契约）。
//   3. **查询路径不得分配**：find_node 是 noexcept 热路径，索引命中时零分配；
//      只有索引失配才走线性扫描（同样零分配）。重建索引要分配内存，
//      因此只放在 ensure_node（本就分配内存）与显式 rebuild_node_index 里。

void AstGraph::rebuild_node_index() {
    node_index_.clear();
    node_index_.reserve(nodes.size());
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        // 用 operator[] 而非 emplace：万一存在重复 key，后写入的应当胜出，
        // 与"nodes 里最后出现者为准"的直觉一致。
        node_index_[nodes[index].key] = index;
    }
}

AstNode* AstGraph::find_node(std::string_view key) noexcept {
    // 索引与 nodes 同步时走哈希路径。size 不一致说明外部直接动过 nodes。
    if (node_index_.size() == nodes.size()) {
        const auto it = node_index_.find(key);
        if (it != node_index_.end()) {
            // 复核 key：外部若原地改写了某个节点的 key（不改 size），索引会指错。
            if (nodes[it->second].key == key) {
                return &nodes[it->second];
            }
        } else {
            return nullptr;
        }
    }
    // 冷路径：索引失配（或 key 被原地改写），退回线性扫描。
    // 线性扫描不分配，因此 noexcept 依然成立。
    for (AstNode& node : nodes) {
        if (node.key == key) {
            return &node;
        }
    }
    return nullptr;
}

// const 版本复用非 const 版本：只在**这一处**去掉 const（节点本身并未被改），
// 而不是让每个调用点各写一遍 const_cast。
const AstNode* AstGraph::find_node(std::string_view key) const noexcept {
    return const_cast<AstGraph*>(this)->find_node(key);
}

bool AstGraph::has_node(std::string_view key) const noexcept { return find_node(key) != nullptr; }

AstNode& AstGraph::ensure_node(std::string_view key) {
    if (AstNode* existing = find_node(key)) {
        return *existing;
    }
    // 索引落后于 nodes（外部直接 push_back 过）：先重建，保证新节点下标正确。
    if (node_index_.size() != nodes.size()) {
        rebuild_node_index();
    }
    AstNode node;
    node.key.assign(key);
    node.label.assign(key);
    nodes.push_back(std::move(node));
    node_index_[nodes.back().key] = nodes.size() - 1;
    return nodes.back();
}

Result<graph::Graph> to_graph(const AstGraph& ast) {
    graph::Graph out;
    // 边端点查表：历史是 vector + 线性扫描（O(N·E)），换成哈希表 O(1)。
    // 用透明哈希：查 string_view 不必先构造临时 std::string（省掉每条边一次堆分配）。
    using NodeIdIndex =
        std::unordered_map<std::string, graph::NodeId, TransparentStringHash, TransparentStringEqual>;
    NodeIdIndex id_of;
    id_of.reserve(ast.nodes.size());

    // 图级属性：以前 AST 的图级属性在转换里被整段丢弃（D2 的顶层 label / 全局
    // style / vars 都因此消失）。现在搬进 Graph 的图级属性袋，渲染侧拿得到。
    for (const AstAttribute& attribute : ast.attributes) {
        out.attributes().set(attribute.key, attribute.value);
    }

    for (const AstNode& ast_node : ast.nodes) {
        graph::Node& node = out.add_node(ast_node.label.empty() ? ast_node.key : ast_node.label);
        if (!ast_node.shape.empty()) {
            node.attributes().set("shape", ast_node.shape);
        }
        for (const AstAttribute& attribute : ast_node.attributes) {
            node.attributes().set(attribute.key, attribute.value);
        }
        id_of[ast_node.key] = node.id();
    }

    const auto lookup = [&id_of](std::string_view key) -> graph::NodeId {
        const auto it = id_of.find(key);
        return it == id_of.end() ? graph::kInvalidNode : it->second;
    };

    for (const AstEdge& ast_edge : ast.edges) {
        const graph::NodeId from = lookup(ast_edge.from);
        const graph::NodeId to = lookup(ast_edge.to);
        if (from == graph::kInvalidNode || to == graph::kInvalidNode) {
            // 解析器保证边端点一定被登记过；走到这里说明 AST 被手工构造错了。
            return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
        }
        graph::Edge* edge = out.add_edge(from, to, ast_edge.label);
        if (edge == nullptr) {
            // add_edge 只在端点不存在时返回 nullptr；上面已查过端点，走到这里属契约破坏。
            return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
        }
        switch (ast_edge.arrow) {
            case ArrowKind::None:
                edge->set_arrow(graph::ArrowStyle::None);
                break;
            case ArrowKind::Both:
                edge->set_arrow(graph::ArrowStyle::Both);
                break;
            case ArrowKind::AtTarget:
                edge->set_arrow(graph::ArrowStyle::AtTarget);
                break;
        }
        if (!ast_edge.line_style.empty()) {
            edge->attributes().set("style", ast_edge.line_style);
        }
        for (const AstAttribute& attribute : ast_edge.attributes) {
            edge->attributes().set(attribute.key, attribute.value);
        }
    }
    return out;
}

}  // namespace termchart::dsl
