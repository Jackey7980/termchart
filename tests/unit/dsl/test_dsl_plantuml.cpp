// test_dsl_plantuml.cpp —— dsl 模块：PlantUML 类图解析
//
// 关注的契约：
//   * `@startuml` / `@enduml` 包裹缺失必须报 ParseError；
//   * 关系操作符的语义（继承/组合/聚合/关联/依赖）以及"箭头在左端"时 from/to 不反；
//   * 未识别的行整行跳过、继续解析，且**不改变已解析出的图结构**。
#include <doctest.h>

#include <string>
#include <string_view>
#include <vector>

#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/plantuml.hpp"
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

std::size_t count_attr(const std::vector<AstAttribute>& attrs, std::string_view key) {
    std::size_t count = 0;
    for (const AstAttribute& attribute : attrs) {
        if (attribute.key == key) {
            ++count;
        }
    }
    return count;
}

}  // namespace

TEST_CASE("dsl/plantuml: @startuml/@enduml 与基本关系") {
    const std::string source = R"(@startuml
        class A
        class B
        A --> B
        @enduml
    )";
    const Result<AstGraph> result = parse_plantuml(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;
    CHECK(ast.directive == "@startuml");
    REQUIRE(ast.nodes.size() == 2);
    REQUIRE(ast.edges.size() == 1);
    CHECK(ast.edges[0].from == "A");
    CHECK(ast.edges[0].to == "B");
    CHECK(ast.edges[0].arrow == ArrowKind::AtTarget);
    const AstAttribute* relation = find_attr(ast.edges[0].attributes, "relation");
    REQUIRE(relation != nullptr);
    CHECK(relation->value == "association");
}

TEST_CASE("dsl/plantuml: <|-- 的继承方向不反") {
    // `A <|-- B` ≡ `B --|> A`：B 继承 A ⇒ from=B，to=A。
    const Result<AstGraph> result = parse_plantuml("@startuml\nA <|-- B\n@enduml\n");
    REQUIRE(result.has_value());
    REQUIRE(result->edges.size() == 1);
    CHECK(result->edges[0].from == "B");
    CHECK(result->edges[0].to == "A");
    CHECK(result->edges[0].arrow == ArrowKind::AtTarget);
    const AstAttribute* relation = find_attr(result->edges[0].attributes, "relation");
    REQUIRE(relation != nullptr);
    CHECK(relation->value == "inheritance");
}

TEST_CASE("dsl/plantuml: 组合/聚合/依赖的语义与线型") {
    const std::string source = R"(@startuml
        A *-- B
        B o-- C
        C ..> D
        D ..|> E
        @enduml
    )";
    const Result<AstGraph> result = parse_plantuml(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;
    REQUIRE(ast.edges.size() == 4);

    CHECK(ast.edges[0].from == "A");
    CHECK(ast.edges[0].to == "B");
    CHECK(ast.edges[0].arrow == ArrowKind::None);
    CHECK(find_attr(ast.edges[0].attributes, "relation")->value == "composition");

    CHECK(ast.edges[1].from == "B");
    CHECK(ast.edges[1].to == "C");
    CHECK(find_attr(ast.edges[1].attributes, "relation")->value == "aggregation");

    CHECK(ast.edges[2].line_style == "dashed");
    CHECK(find_attr(ast.edges[2].attributes, "relation")->value == "dependency");

    CHECK(ast.edges[3].line_style == "dashed");
    CHECK(find_attr(ast.edges[3].attributes, "relation")->value == "inheritance");
}

TEST_CASE("dsl/plantuml: 关系标签与多重度") {
    const Result<AstGraph> result =
        parse_plantuml("@startuml\nA \"1\" --> \"*\" B : uses\n@enduml\n");
    REQUIRE(result.has_value());
    REQUIRE(result->edges.size() == 1);
    const AstEdge& edge = result->edges[0];
    CHECK(edge.from == "A");
    CHECK(edge.to == "B");
    CHECK(edge.label == "uses");
    const AstAttribute* from_mult = find_attr(edge.attributes, "from_multiplicity");
    const AstAttribute* to_mult = find_attr(edge.attributes, "to_multiplicity");
    REQUIRE(from_mult != nullptr);
    REQUIRE(to_mult != nullptr);
    CHECK(from_mult->value == "1");
    CHECK(to_mult->value == "*");
}

TEST_CASE("dsl/plantuml: 类体成员原样保留、不解析语法") {
    const std::string source = R"(@startuml
        class A {
            + int x
            - void y()
        }
        @enduml
    )";
    const Result<AstGraph> result = parse_plantuml(source);
    REQUIRE(result.has_value());
    const AstNode* node = result->find_node("A");
    REQUIRE(node != nullptr);
    REQUIRE(count_attr(node->attributes, "member") == 2);
    CHECK(node->attributes[0].value == "+ int x");
    CHECK(node->attributes[1].value == "- void y()");
}

TEST_CASE("dsl/plantuml: class ... as 别名") {
    const std::string source = R"(@startuml
        class Foo as F
        F --> Bar
        @enduml
    )";
    const Result<AstGraph> result = parse_plantuml(source);
    REQUIRE(result.has_value());
    const AstNode* node = result->find_node("F");
    REQUIRE(node != nullptr);
    CHECK(node->label == "Foo");
    CHECK(result->find_node("Bar") != nullptr);
    REQUIRE(result->edges.size() == 1);
    CHECK(result->edges[0].from == "F");
}

TEST_CASE("dsl/plantuml: note/skinparam/title 被忽略且不产生节点") {
    const std::string source = R"(@startuml
        title 订单
        skinparam monochrome true
        note over A : 提示
        A --> B
        @enduml
    )";
    const Result<AstGraph> result = parse_plantuml(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;
    CHECK(ast.find_node("title") == nullptr);
    CHECK(ast.find_node("skinparam") == nullptr);
    CHECK(ast.find_node("note") == nullptr);
    CHECK(ast.nodes.size() == 2);
    CHECK(ast.edges.size() == 1);
}

TEST_CASE("dsl/plantuml: 多行 note 块不改变图结构") {
    const std::string source = R"(@startuml
        note over A
        A --> B
        end note
        @enduml
    )";
    const Result<AstGraph> result = parse_plantuml(source);
    REQUIRE(result.has_value());
    CHECK(result->edges.empty());  // note 块里的 A --> B 属于注释内容，不得变成边
    CHECK(result->nodes.empty());
}

TEST_CASE("dsl/plantuml: 未识别行忽略并继续") {
    const std::string source = R"(@startuml
        hide members
        Foo.Bar baz
        A --> B
        @enduml
    )";
    const Result<AstGraph> result = parse_plantuml(source);
    REQUIRE(result.has_value());
    CHECK(result->edges.size() == 1);
    CHECK(result->nodes.size() == 2);
}

TEST_CASE("dsl/plantuml: 错误路径给出错误码且不抛异常") {
    SUBCASE("缺 @startuml") {
        const Result<AstGraph> result = parse_plantuml("class A\n");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::ParseError);
    }
    SUBCASE("缺 @enduml") {
        const Result<AstGraph> result = parse_plantuml("@startuml\nA --> B\n");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::ParseError);
    }
    SUBCASE("节点数上限") {
        PlantUmlParseOptions options;
        options.max_nodes = 1;
        const Result<AstGraph> result = parse_plantuml("@startuml\nA --> B\n@enduml", options);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::OutOfRange);
    }
}

TEST_CASE("dsl/plantuml: AST 落成 graph::Graph") {
    const Result<AstGraph> ast = parse_plantuml("@startuml\nA \"1\" --> \"*\" B\n@enduml\n");
    REQUIRE(ast.has_value());
    const Result<graph::Graph> produced = to_graph(*ast);
    REQUIRE(produced.has_value());
    REQUIRE(produced->nodes().size() == 2);
    REQUIRE(produced->edges().size() == 1);
    CHECK(produced->edges()[0].attributes().get("relation") == "association");
}
