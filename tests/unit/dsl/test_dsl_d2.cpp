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

// ── 块级声明（style / vars / classes）────────────────────────────────────
//
// 回归：这三块历史上被"整块跳过"或"误当容器块造幽灵节点"。这里把正确语义钉死。

TEST_CASE("dsl/d2: style 块内容不再丢失，对所有节点生效") {
    const std::string source = R"(a: circle
style: {
  fill: "#f00"
  stroke: black
}
b
)";
    const Result<AstGraph> result = parse_d2(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;

    // 仍然不造节点
    CHECK(ast.nodes.size() == 2);
    CHECK(ast.find_node("style") == nullptr);

    const AstNode* node_a = ast.find_node("a");
    const AstNode* node_b = ast.find_node("b");
    REQUIRE(node_a != nullptr);
    REQUIRE(node_b != nullptr);

    // 块内属性逐条落到节点上（引号已剥）
    bool a_fill = false;
    bool a_stroke = false;
    for (const AstAttribute& attribute : node_a->attributes) {
        a_fill = a_fill || (attribute.key == "style.fill" && attribute.value == "#f00");
        a_stroke = a_stroke || (attribute.key == "style.stroke" && attribute.value == "black");
    }
    CHECK(a_fill);
    CHECK(a_stroke);

    // 顶层 style 与声明顺序无关：b 在 style 之后声明，同样拿到样式
    bool b_fill = false;
    for (const AstAttribute& attribute : node_b->attributes) {
        b_fill = b_fill || (attribute.key == "style.fill" && attribute.value == "#f00");
    }
    CHECK(b_fill);

    // 图级属性袋里也留一份（AST → Graph 时会被搬进 Graph 的图级属性）
    bool graph_fill = false;
    for (const AstAttribute& attribute : ast.attributes) {
        graph_fill = graph_fill || (attribute.key == "style.fill" && attribute.value == "#f00");
    }
    CHECK(graph_fill);
}

TEST_CASE("dsl/d2: vars / classes 块不再造幽灵节点") {
    const std::string source = R"(vars: {
  x: 5
}
classes: {
  warn: {
    fill: yellow
  }
}
a
)";
    const Result<AstGraph> result = parse_d2(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;

    // 关键回归：以前这里会造出名为 vars / classes 的容器节点，
    // 甚至把 vars 块里的 x 解析成节点。
    CHECK(ast.nodes.size() == 1);
    CHECK(ast.find_node("vars") == nullptr);
    CHECK(ast.find_node("classes") == nullptr);
    CHECK(ast.find_node("x") == nullptr);
    CHECK(ast.find_node("warn") == nullptr);
    CHECK(ast.find_node("a") != nullptr);

    bool has_var = false;
    bool has_class = false;
    for (const AstAttribute& attribute : ast.attributes) {
        has_var = has_var || (attribute.key == "var.x" && attribute.value == "5");
        has_class = has_class || (attribute.key == "class.warn.fill" && attribute.value == "yellow");
    }
    CHECK(has_var);
    CHECK(has_class);
}

TEST_CASE("dsl/d2: 类样式套用到引用它的节点，节点自有样式优先") {
    const std::string source = R"(classes: {
  warn: {
    fill: yellow
    stroke: red
  }
}
a.classes: warn
b.classes: warn
b.style: {
  fill: green
}
)";
    const Result<AstGraph> result = parse_d2(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;
    REQUIRE(ast.nodes.size() == 2);

    const AstNode* a = ast.find_node("a");
    const AstNode* b = ast.find_node("b");
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);

    bool a_fill_yellow = false;
    bool a_stroke_red = false;
    for (const AstAttribute& attribute : a->attributes) {
        a_fill_yellow = a_fill_yellow || (attribute.key == "style.fill" && attribute.value == "yellow");
        a_stroke_red = a_stroke_red || (attribute.key == "style.stroke" && attribute.value == "red");
    }
    CHECK(a_fill_yellow);
    CHECK(a_stroke_red);

    // b 自己写了 fill: green —— 节点自有样式压过类样式
    bool b_fill_green = false;
    bool b_fill_yellow = false;
    bool b_stroke_red = false;
    for (const AstAttribute& attribute : b->attributes) {
        b_fill_green = b_fill_green || (attribute.key == "style.fill" && attribute.value == "green");
        b_fill_yellow = b_fill_yellow || (attribute.key == "style.fill" && attribute.value == "yellow");
        b_stroke_red = b_stroke_red || (attribute.key == "style.stroke" && attribute.value == "red");
    }
    CHECK(b_fill_green);
    CHECK_FALSE(b_fill_yellow);
    CHECK(b_stroke_red);
}

TEST_CASE("dsl/d2: 块级声明缺 '{' 时报错而不是静默吞掉") {
    // fail-loud：style 是 D2 保留字，写成 `style: circle` 是笔误，
    // 以前整行被静默跳过，现在必须报错。
    const Result<AstGraph> bad = parse_d2("style: circle\n");
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error() == ErrorCode::ParseError);
}

}  // namespace
