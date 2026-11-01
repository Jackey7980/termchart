// test_dsl_dot.cpp —— dsl 模块：Graphviz DOT 解析
//
// 关注契约而非"能解析一句话"：
//   * 引号里的 `->` 绝不能被当成箭头（DOT 属性值里出现箭头是最常见的坑）；
//   * `node [...]` / `edge [...]` 全局默认属性对所有节点/边生效，且显式属性优先；
//   * 认得出但不做的指令（subgraph）不产生节点、不报错；
//   * 错误路径（缺头部/缺闭合/超 max_nodes）给出错误码且不抛异常。
#include <doctest.h>

#include <string>
#include <string_view>
#include <vector>

#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/graphviz_dot.hpp"
#include "termchart/graph/graph.hpp"

namespace {

using namespace termchart;
using namespace termchart::dsl;

const AstAttribute* find_attr(const std::vector<AstAttribute>& attrs, std::string_view key) {
    for (const AstAttribute& attribute : attrs) {
        if (attribute.key == key) {
            return &attribute;
        }
    }
    return nullptr;
}

// 自己扫 nodes 找节点：测试只依赖 nodes 这个唯一真源，不依赖 AstGraph 的查询重载。
const AstNode* find_node_by_key(const AstGraph& ast, std::string_view key) {
    for (const AstNode& node : ast.nodes) {
        if (node.key == key) {
            return &node;
        }
    }
    return nullptr;
}

int count_attr(const std::vector<AstAttribute>& attrs, std::string_view key) {
    int count = 0;
    for (const AstAttribute& attribute : attrs) {
        if (attribute.key == key) {
            ++count;
        }
    }
    return count;
}

}  // namespace

TEST_CASE("dsl/dot: 基本有向图、图名与 rankdir") {
    const std::string source = R"(strict digraph G {
        rankdir=LR;
        a -> b;
        b -> c [label="x"];
    })";
    const Result<AstGraph> result = parse_dot(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;
    CHECK(ast.directive == "digraph");
    CHECK(ast.name == "G");
    CHECK(ast.direction == "LR");
    REQUIRE(ast.nodes.size() == 3);

    REQUIRE(ast.edges.size() == 2);
    CHECK(ast.edges[0].from == "a");
    CHECK(ast.edges[0].to == "b");
    CHECK(ast.edges[0].arrow == ArrowKind::AtTarget);
    CHECK(ast.edges[1].from == "b");
    CHECK(ast.edges[1].to == "c");
    CHECK(ast.edges[1].label == "x");

    SUBCASE("边链 a -> b -> c 展开为两条边") {
        const Result<AstGraph> chain = parse_dot("digraph { a -> b -> c }");
        REQUIRE(chain.has_value());
        REQUIRE(chain->edges.size() == 2);
        CHECK(chain->edges[0].from == "a");
        CHECK(chain->edges[0].to == "b");
        CHECK(chain->edges[1].from == "b");
        CHECK(chain->edges[1].to == "c");
    }
}

TEST_CASE("dsl/dot: 无向 graph 的 -- 边") {
    const Result<AstGraph> result = parse_dot("graph { a -- b }");
    REQUIRE(result.has_value());
    CHECK(result->directive == "graph");
    REQUIRE(result->edges.size() == 1);
    CHECK(result->edges[0].arrow == ArrowKind::None);
}

TEST_CASE("dsl/dot: 引号里的 -> 不被当作箭头") {
    const std::string source = R"(digraph {
        a [label="x->y"]
        a -> b
    })";
    const Result<AstGraph> result = parse_dot(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;
    REQUIRE(ast.nodes.size() == 2);
    REQUIRE(ast.edges.size() == 1);  // 若把引号里的 -> 当箭头，这里会变成 2
    const AstNode* node = ast.find_node("a");
    REQUIRE(node != nullptr);
    CHECK(node->label == "x->y");
}

TEST_CASE("dsl/dot: 属性列表 label/shape 与未知 shape 不静默丢弃") {
    const std::string source = R"(digraph {
        a [label="节点", shape=box, color=red]
        b [shape=star]
        c [shape=ellipse]
    })";
    const Result<AstGraph> result = parse_dot(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;

    const AstNode* a = ast.find_node("a");
    REQUIRE(a != nullptr);
    CHECK(a->label == "节点");
    CHECK(a->shape == "box");
    const AstAttribute* color = find_attr(a->attributes, "color");
    REQUIRE(color != nullptr);
    CHECK(color->value == "red");

    const AstNode* b = ast.find_node("b");
    REQUIRE(b != nullptr);
    CHECK(b->shape.empty());                              // star 不在支持集
    CHECK(find_attr(b->attributes, "shape") != nullptr);  // 但原样保留

    CHECK(ast.find_node("c")->shape == "ellipse");
}

TEST_CASE("dsl/dot: 花括号并列节点集展开为多条边") {
    SUBCASE("右侧集合") {
        const Result<AstGraph> result = parse_dot("digraph { a -> {b c} }");
        REQUIRE(result.has_value());
        REQUIRE(result->edges.size() == 2);
        CHECK(result->edges[0].to == "b");
        CHECK(result->edges[1].to == "c");
    }
    SUBCASE("左侧集合") {
        const Result<AstGraph> result = parse_dot("digraph { {a b} -> c }");
        REQUIRE(result.has_value());
        REQUIRE(result->edges.size() == 2);
        CHECK(result->edges[0].from == "a");
        CHECK(result->edges[1].from == "b");
    }
}

TEST_CASE("dsl/dot: 认得出但不做的指令不产生节点") {
    const std::string source = R"(digraph {
        graph [rankdir=LR];
        node [shape=box];
        edge [color=red];
        subgraph cluster_0 {
            a -> b;
        }
        a -> c;
    })";
    const Result<AstGraph> result = parse_dot(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;
    CHECK(ast.direction == "LR");
    CHECK(ast.find_node("graph") == nullptr);
    CHECK(ast.find_node("node") == nullptr);
    CHECK(ast.find_node("edge") == nullptr);
    CHECK(ast.nodes.size() == 3);  // a / b / c
    CHECK(ast.edges.size() == 2);
}

TEST_CASE("dsl/dot: 错误路径给出定位且不抛异常") {
    SUBCASE("缺头部") {
        ParseDiagnostic diagnostic;
        const Result<AstGraph> result = parse_dot("a -> b", {}, &diagnostic);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::ParseError);
        CHECK(diagnostic.line == 1);
        CHECK_FALSE(diagnostic.message.empty());
    }
    SUBCASE("缺闭合 }") {
        const Result<AstGraph> result = parse_dot("digraph G { a -> b");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::ParseError);
    }
    SUBCASE("节点数上限") {
        DotParseOptions options;
        options.max_nodes = 2;
        const Result<AstGraph> result = parse_dot("digraph { a -> b; b -> c }", options);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::OutOfRange);
    }
}

TEST_CASE("dsl/dot: AST 落成 graph::Graph 且不丢信息") {
    const Result<AstGraph> ast = parse_dot("digraph { a [label=\"读\"]; a -> b [label=\"ok\"] }");
    REQUIRE(ast.has_value());
    const Result<graph::Graph> produced = to_graph(*ast);
    REQUIRE(produced.has_value());
    REQUIRE(produced->nodes().size() == 2);
    REQUIRE(produced->edges().size() == 1);
    CHECK(produced->nodes()[0].label() == "读");
    CHECK(produced->edges()[0].label() == "ok");
}

// `node [...]` / `edge [...]` 是 DOT 的全局默认属性：对所有节点/边生效（不论声明
// 先后），且**显式属性优先**。旧实现整行丢弃这些默认样式（只认 graph[rankdir]）。
TEST_CASE("dsl/dot: node/edge 全局默认属性生效，且显式属性优先") {
    const std::string source = R"(digraph G {
        node [shape=box color=red];
        edge [style=dashed];
        a;
        b [shape=diamond, color=blue];
        a -> b;
    })";
    const Result<AstGraph> result = parse_dot(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;

    // 默认属性不制造幽灵节点
    CHECK(ast.nodes.size() == 2);

    const AstNode* a = find_node_by_key(ast, "a");
    const AstNode* b = find_node_by_key(ast, "b");
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);

    // 未显式声明 shape 的节点套用默认 shape
    CHECK(a->shape == "box");
    // 显式属性优先，不被默认值反向覆盖
    CHECK(b->shape == "diamond");

    // 默认 color 只落到没显式写 color 的节点上；显式写了的不该再叠一份默认值
    const AstAttribute* a_color = find_attr(a->attributes, "color");
    REQUIRE(a_color != nullptr);
    CHECK(a_color->value == "red");
    const AstAttribute* b_color = find_attr(b->attributes, "color");
    REQUIRE(b_color != nullptr);
    CHECK(b_color->value == "blue");
    CHECK(count_attr(b->attributes, "color") == 1);  // 没有"默认+显式"两份

    // 边默认属性
    REQUIRE(ast.edges.size() == 1);
    CHECK(ast.edges[0].line_style == "dashed");

    // 默认属性也记进图属性袋（ast.hpp 约定：绝不静默丢弃）
    const AstAttribute* bagged = find_attr(ast.attributes, "default_node_attr");
    REQUIRE(bagged != nullptr);
}
