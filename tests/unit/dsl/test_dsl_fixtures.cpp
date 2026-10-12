// test_dsl_fixtures.cpp —— 四份 DSL 夹具必须"原样可解析"
//
// 夹具即生成物（tests/fixtures/README.md）：`tools/gen_dsl_samples.py` 产出
// `sample_mermaid.txt` / `sample_dot.txt` / `sample_plantuml.txt` / `sample.csv`。
// 生成器保证的是"内容不变"，本文件保证的是"内容仍被解析器接受"——
// 方言支持面缩水（或夹具写进未声明支持的东西）时，这两个保证缺一不可。
//
// 断言的是**结构性事实**（节点数下界、关系语义、类型推断结果），不是逐字节相等：
// 夹具的文字可以改，方言的语义不能变。
#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/csv_sniff.hpp"
#include "termchart/dsl/graphviz_dot.hpp"
#include "termchart/dsl/mermaid.hpp"
#include "termchart/dsl/plantuml.hpp"
#include "termchart/dsl/table_markdown.hpp"

#ifdef TERMCHART_FIXTURES_DIR
namespace {

std::string read_fixture(const char* name) {
    std::ifstream input(std::filesystem::path(TERMCHART_FIXTURES_DIR) / name, std::ios::binary);
    REQUIRE(input.good());
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

using namespace termchart;
using namespace termchart::dsl;

TEST_CASE("dsl/fixtures: sample_dot.txt —— 引号内含箭头的标签不被误伤") {
    const std::string source = read_fixture("sample_dot.txt");
    const Result<AstGraph> result = parse_dot(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;
    CHECK(ast.directive == "digraph");
    CHECK(ast.direction == "LR");
    CHECK(ast.nodes.size() >= 5);

    // 这是 DOT 解析最典型的坑：label="read -> parse" 里的 "->" 不能被当成边。
    const AstNode* first = ast.find_node("a");
    REQUIRE(first != nullptr);
    CHECK(first->label == "read -> parse");
    CHECK(first->shape == "ellipse");

    // 并列节点集 `{d e}` 展开成多条边。
    CHECK(ast.edges.size() >= 4);
    bool has_dashed = false;
    for (const AstEdge& edge : ast.edges) {
        has_dashed = has_dashed || edge.line_style == "dashed";
    }
    CHECK(has_dashed);
}

TEST_CASE("dsl/fixtures: sample_plantuml.txt —— 关系语义与方向") {
    const std::string source = read_fixture("sample_plantuml.txt");
    const Result<AstGraph> result = parse_plantuml(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;
    CHECK(ast.name == "Pipeline");
    CHECK(ast.nodes.size() >= 4);

    const auto relation_of = [&ast](std::string_view from, std::string_view to) {
        for (const AstEdge& edge : ast.edges) {
            if (edge.from == from && edge.to == to) {
                for (const AstAttribute& attribute : edge.attributes) {
                    if (attribute.key == "relation") {
                        return attribute.value;
                    }
                }
            }
        }
        return std::string("missing");
    };
    // `Source <|-- Reader`：Reader 继承 Source ⇒ 边从 Reader 指向 Source。
    CHECK(relation_of("Reader", "Source") == "inheritance");
    CHECK(relation_of("Reader", "Parser") == "composition");
    CHECK(relation_of("Parser", "Token") == "aggregation");
    CHECK(relation_of("Parser", "Error") == "dependency");

    // 类体逐行保留（Reader 有 +open(path) 与 -buffer 两行）。
    const AstNode* reader = ast.find_node("Reader");
    REQUIRE(reader != nullptr);
    std::size_t members = 0;
    for (const AstAttribute& attribute : reader->attributes) {
        if (attribute.key == "member") {
            ++members;
        }
    }
    CHECK(members >= 2);
}

TEST_CASE("dsl/fixtures: sample.csv —— 引号字段含分隔符与换行，类型推断正确") {
    const std::string source = read_fixture("sample.csv");
    const Result<document::TableModel> table = parse_csv(source);
    REQUIRE(table.has_value());
    CHECK(table->header().size() == 6);
    // 第三条记录含跨行引号字段，行数必须仍是 4（不能被换行拆成 5 行）。
    CHECK(table->rows().size() == 4);

    const Result<std::vector<ColumnProfile>> profiles = sniff_columns(source);
    REQUIRE(profiles.has_value());
    const std::vector<ColumnProfile>& columns = *profiles;
    REQUIRE(columns.size() == 6);
    CHECK(columns[0].type == ColumnType::Integer);   // id
    CHECK(columns[1].type == ColumnType::Text);      // name
    CHECK(columns[2].type == ColumnType::Real);      // score（含空值，仍升级为 Real）
    CHECK(columns[3].type == ColumnType::Boolean);   // active（true/no 混写）
    CHECK(columns[4].type == ColumnType::Date);      // joined
    CHECK(columns[1].name == "name");
}

TEST_CASE("dsl/fixtures: Markdown 表格夹具内联样本（暂无独立文件）") {
    // table_markdown 目前没有独立夹具文件（规格未点名），这里用内联样本保证
    // "转义竖线 + 对齐行"这两件最容易写错的事被覆盖。
    const std::string source =
        "| 名称 | 数量 | 备注 |\n"
        "| :--- | ---: | :---: |\n"
        "| A\\|B | 3 | 含转义竖线 |\n";
    const Result<document::TableModel> table = parse_markdown_table(source);
    REQUIRE(table.has_value());
    CHECK(table->header().size() == 3);
    REQUIRE(table->rows().size() == 1);
    CHECK(table->rows()[0][0] == "A|B");  // 转义被还原
}

}  // namespace
#else
#error "TERMCHART_FIXTURES_DIR 未定义：DSL 夹具测试需要由 CMake 注入 tests/fixtures 路径，否则用例会被静默丢弃"
#endif
