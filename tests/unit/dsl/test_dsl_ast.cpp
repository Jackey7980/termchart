// test_dsl_ast.cpp —— AST 中立模型：节点索引 / 图级属性 / to_graph 转换
//
// 回归背景：
//   * AST 的 find_node / ensure_node 历史上是**线性扫描**（O(N)），建图退化成 O(N²)；
//     现在改成 key → 下标的哈希索引，但必须保持"外部直接改 nodes 也不会失灵"。
//   * AST 的**图级属性**（D2 的全局 style / vars / 类定义等）历史上在 to_graph
//     里被整段丢弃；现在搬进 Graph 的图级属性袋。
#include <doctest.h>

#include <string>
#include <vector>

#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/d2.hpp"
#include "termchart/graph/graph.hpp"

namespace {

using namespace termchart;
using namespace termchart::dsl;

TEST_CASE("dsl/ast: ensure_node / find_node 行为与线性实现等价") {
    AstGraph ast;

    // 首次创建
    AstNode& first = ast.ensure_node("alpha");
    CHECK(first.key == "alpha");
    CHECK(first.label == "alpha");

    // 再次 ensure 同一 key：返回**同一个**节点（不是新副本）
    AstNode& again = ast.ensure_node("alpha");
    CHECK(&again == &first);
    CHECK(ast.nodes.size() == 1);

    // 不同 key 各自成节点，顺序即创建顺序
    ast.ensure_node("beta");
    ast.ensure_node("gamma");
    REQUIRE(ast.nodes.size() == 3);
    CHECK(ast.nodes[0].key == "alpha");
    CHECK(ast.nodes[1].key == "beta");
    CHECK(ast.nodes[2].key == "gamma");

    CHECK(ast.find_node("beta") != nullptr);
    CHECK(ast.find_node("missing") == nullptr);
    CHECK(ast.has_node("gamma"));
    CHECK_FALSE(ast.has_node("nope"));
}

TEST_CASE("dsl/ast: 哈希索引下外部直接改 nodes 仍能查到") {
    // mermaid_flow 等方言会绕过 ensure_node 直接 push_back。
    // 索引必须能自愈，否则这些节点会"查不到"。
    AstGraph ast;
    ast.ensure_node("a");

    AstNode extra;
    extra.key = "direct";
    extra.label = "direct";
    ast.nodes.push_back(std::move(extra));

    const AstNode* found = ast.find_node("direct");
    REQUIRE(found != nullptr);
    CHECK(found->key == "direct");

    // 自愈之后再 ensure 也不能把新节点写歪
    ast.ensure_node("b");
    CHECK(ast.nodes.size() == 3);
    CHECK(ast.find_node("a") != nullptr);
    CHECK(ast.find_node("b") != nullptr);
    CHECK(ast.find_node("direct") != nullptr);
}

TEST_CASE("dsl/ast: 重建索引后查找结果不变") {
    AstGraph ast;
    ast.ensure_node("x");
    ast.ensure_node("y");
    ast.rebuild_node_index();
    CHECK(ast.find_node("x") != nullptr);
    CHECK(ast.find_node("y") != nullptr);
    CHECK(ast.find_node("z") == nullptr);
    CHECK(ast.nodes.size() == 2);
}

TEST_CASE("dsl/ast: to_graph 保留图级属性（不再整段丢弃）") {
    AstGraph ast;
    ast.name = "demo";
    ast.attributes.push_back(AstAttribute{"style.fill", "#f00"});
    ast.attributes.push_back(AstAttribute{"var.x", "5"});

    AstNode& node = ast.ensure_node("a");
    node.attributes.push_back(AstAttribute{"d2_shape", "circle"});

    AstEdge edge;
    edge.from = "a";
    edge.to = "a";
    ast.edges.push_back(edge);

    const Result<graph::Graph> result = to_graph(ast);
    REQUIRE(result.has_value());
    const graph::Graph& graph = *result;

    CHECK(graph.attributes().get("style.fill") == "#f00");
    CHECK(graph.attributes().get("var.x") == "5");

    // 节点属性照旧
    REQUIRE(graph.node_count() == 1);
    CHECK(graph.nodes()[0].attributes().get("d2_shape") == "circle");
}

TEST_CASE("dsl/ast: to_graph 拒绝引用不存在端点的边") {
    AstGraph ast;
    ast.ensure_node("a");
    AstEdge edge;
    edge.from = "a";
    edge.to = "ghost";
    ast.edges.push_back(edge);

    const Result<graph::Graph> result = to_graph(ast);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvalidArgument);
}

TEST_CASE("dsl/ast: D2 解析出的图级属性能一路走到 Graph") {
    const std::string source = R"(style: {
  fill: "#f00"
}
a
)";
    const Result<AstGraph> parsed = parse_d2(source);
    REQUIRE(parsed.has_value());
    const Result<graph::Graph> graph = to_graph(*parsed);
    REQUIRE(graph.has_value());
    CHECK(graph->attributes().get("style.fill") == "#f00");
}

}  // namespace
