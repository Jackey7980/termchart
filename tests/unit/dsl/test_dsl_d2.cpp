// test_dsl_d2.cpp —— D2 方言：节点形状 / 连接 / 容器 / 属性
#include <doctest.h>

#include <string>

#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/d2.hpp"
#include "termchart/dsl/dsl_registry.hpp"

namespace {

using namespace termchart;
using namespace termchart::dsl;

TEST_CASE("dsl/d2: 节点形状声明映射到本库形状词汇") {
    const std::string source = R"(direction: right
a: circle
b: oval
c: diamond
d: hexagon
e: parallelogram
f: rectangle
)";
    const Result<AstGraph> result = parse_d2(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;
    CHECK(ast.direction == "LR");
    CHECK(ast.find_node("a")->shape == "circle");
    CHECK(ast.find_node("b")->shape == "round");
    CHECK(ast.find_node("c")->shape == "diamond");
    CHECK(ast.find_node("d")->shape == "hexagon");
    CHECK(ast.find_node("e")->shape == "asymmetric");
    CHECK(ast.find_node("f")->shape == "box");

    SUBCASE("原形状名不丢（渲染侧可自行解释）") {
        const AstNode* node = ast.find_node("f");
        REQUIRE(node != nullptr);
        bool recorded = false;
        for (const AstAttribute& attribute : node->attributes) {
            recorded = recorded || (attribute.key == "d2_shape" && attribute.value == "rectangle");
        }
        CHECK(recorded);
    }
}

TEST_CASE("dsl/d2: 连接的三种形态与链") {
    const std::string source = R"(a -> b: 主链路
b <- c
c -- d
d -> e -> f
)";
    const Result<AstGraph> result = parse_d2(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;
    REQUIRE(ast.edges.size() == 5);

    CHECK(ast.edges[0].from == "a");
    CHECK(ast.edges[0].to == "b");
    CHECK(ast.edges[0].label == "主链路");

    // `b <- c`：方向反转 ⇒ 边从 c 指向 b。
    CHECK(ast.edges[1].from == "c");
    CHECK(ast.edges[1].to == "b");

    // `c -- d`：无箭头。
    CHECK(ast.edges[2].arrow == ArrowKind::None);

    // 链 `d -> e -> f` 展开成两条边。
    CHECK(ast.edges[3].from == "d");
    CHECK(ast.edges[3].to == "e");
    CHECK(ast.edges[4].from == "e");
    CHECK(ast.edges[4].to == "f");
}

TEST_CASE("dsl/d2: 容器块与属性归属") {
    const std::string source = R"(cloud: {
  a: circle
  b
  a -> b
}
a -> c
)";
    const Result<AstGraph> result = parse_d2(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;

    const AstNode* cloud = ast.find_node("cloud");
    REQUIRE(cloud != nullptr);
    bool is_container = false;
    for (const AstAttribute& attribute : cloud->attributes) {
        is_container = is_container || attribute.key == "container";
    }
    CHECK(is_container);

    const AstNode* inside = ast.find_node("a");
    REQUIRE(inside != nullptr);
    REQUIRE_FALSE(inside->attributes.empty());
    CHECK(inside->attributes[0].key == "parent");
    CHECK(inside->attributes[0].value == "cloud");
    CHECK(ast.find_node("c")->attributes.empty());  // 容器外不受影响

    SUBCASE("块内 label 作用于块自身") {
        const Result<AstGraph> labelled = parse_d2("group: {\n  label: 分组\n  x\n}\n");
        REQUIRE(labelled.has_value());
        CHECK(labelled->find_node("group")->label == "分组");
        CHECK(labelled->find_node("group")->label != "group");
    }
    SUBCASE("顶层 label 作用于最近声明的节点") {
        const Result<AstGraph> labelled = parse_d2("x\nlabel: 改名\n");
        REQUIRE(labelled.has_value());
        CHECK(labelled->find_node("x")->label == "改名");
    }
    SUBCASE("name.prop: value 显式指定节点") {
        const Result<AstGraph> explicit_prop = parse_d2("a.tooltip: 提示\na\n");
        REQUIRE(explicit_prop.has_value());
        const AstNode* node = explicit_prop->find_node("a");
        REQUIRE(node != nullptr);
        bool tooltip = false;
        for (const AstAttribute& attribute : node->attributes) {
            tooltip = tooltip || (attribute.key == "tooltip" && attribute.value == "提示");
        }
        CHECK(tooltip);
    }
}

TEST_CASE("dsl/d2: style 块被跳过且不产生节点") {
    const std::string source = R"(a: circle
style: {
  fill: "#f00"
  stroke: black
}
b
)";
    const Result<AstGraph> result = parse_d2(source);
    REQUIRE(result.has_value());
    CHECK(result->nodes.size() == 2);
    CHECK(result->find_node("style") == nullptr);
}

TEST_CASE("dsl/d2: 错误路径") {
    SUBCASE("多余的 }") {
        const Result<AstGraph> bad = parse_d2("a\n}\n");
        REQUIRE_FALSE(bad.has_value());
        CHECK(bad.error() == ErrorCode::ParseError);
    }
    SUBCASE("'.' 后缺少属性名") {
        ParseDiagnostic diagnostic;
        const Result<AstGraph> bad = parse_d2("a.: b\n", {}, &diagnostic);
        REQUIRE_FALSE(bad.has_value());
        CHECK(diagnostic.line == 1);
    }
    SUBCASE("连接箭头后缺少目标") {
        const Result<AstGraph> bad = parse_d2("a ->\n");
        REQUIRE_FALSE(bad.has_value());
    }
    SUBCASE("节点数上限") {
        D2ParseOptions options;
        options.max_nodes = 2;
        const Result<AstGraph> bad = parse_d2("a\nb\nc\n", options);
        REQUIRE_FALSE(bad.has_value());
        CHECK(bad.error() == ErrorCode::OutOfRange);
    }
}

TEST_CASE("dsl/registry: d2 已登记（规格 14 个方言文件就此齐备）") {
    DslRegistry& registry = DslRegistry::instance();
    (void)register_builtin_dsls();
    const DslDescriptor* descriptor = registry.find("d2");
    REQUIRE(descriptor != nullptr);
    CHECK(registry.parse("d2", "a -> b").has_value());
}

}  // namespace
