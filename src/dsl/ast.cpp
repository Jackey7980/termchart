#include "termchart/dsl/ast.hpp"

#include "termchart/graph/edge.hpp"
#include "termchart/graph/graph.hpp"

namespace termchart::dsl {

const AstNode* AstGraph::find_node(std::string_view key) const noexcept {
    for (const AstNode& node : nodes) {
        if (node.key == key) {
            return &node;
        }
    }
    return nullptr;
}

bool AstGraph::has_node(std::string_view key) const noexcept { return find_node(key) != nullptr; }

AstNode& AstGraph::ensure_node(std::string_view key) {
    if (AstNode* existing = const_cast<AstNode*>(find_node(key))) {
        return *existing;
    }
    AstNode node;
    node.key.assign(key);
    node.label.assign(key);
    nodes.push_back(std::move(node));
    return nodes.back();
}

Result<graph::Graph> to_graph(const AstGraph& ast) {
    graph::Graph out;
    std::vector<std::pair<std::string, graph::NodeId>> id_of;
    id_of.reserve(ast.nodes.size());

    for (const AstNode& ast_node : ast.nodes) {
        graph::Node& node = out.add_node(ast_node.label.empty() ? ast_node.key : ast_node.label);
        if (!ast_node.shape.empty()) {
            node.attributes().set("shape", ast_node.shape);
        }
        for (const AstAttribute& attribute : ast_node.attributes) {
            node.attributes().set(attribute.key, attribute.value);
        }
        id_of.emplace_back(ast_node.key, node.id());
    }

    const auto lookup = [&id_of](std::string_view key) -> graph::NodeId {
        for (const auto& [name, id] : id_of) {
            if (name == key) {
                return id;
            }
        }
        return graph::kInvalidNode;
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
