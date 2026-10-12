// test_dsl_csv.cpp —— dsl 模块：markdown 表格解析 + CSV 解析/列类型推断
//
// 关注的不是"能不能解析一句话"，而是**契约**：
//   * 转义（\|）、代码段（`…`）、引号（"…"）这些最容易错解的地方必须正确；
//   * 非法输入不抛异常、不死循环、错误定位准确（未闭合引号指向起始引号）；
//   * 类型推断遵循 Boolean → Integer → Real → Date → Text 的优先级。
#include <doctest.h>

#include <string>
#include <vector>

#include "termchart/dsl/csv_sniff.hpp"
#include "termchart/dsl/table_markdown.hpp"

namespace {

using namespace termchart;
using namespace termchart::dsl;

TEST_CASE("dsl/markdown: 基本管道表格与对齐") {
    const std::string source =
        "| 名称 | 数量 | 价格 |\n"
        "| :--- | :--: | ---: |\n"
        "| 苹果 | 3 | 1.5 |\n"
        "| 梨 | 10 | 2.0 |\n";

    TableAlignment alignment;
    ParseDiagnostic diagnostic;
    const Result<document::TableModel> result =
        parse_markdown_table(source, {}, &diagnostic, &alignment);
    REQUIRE(result.has_value());
    const document::TableModel& table = *result;

    REQUIRE(table.header().size() == 3);
    CHECK(table.header()[0] == "名称");
    CHECK(table.header()[1] == "数量");
    CHECK(table.header()[2] == "价格");
    REQUIRE(table.row_count() == 2);
    CHECK(table.rows()[0][0] == "苹果");
    CHECK(table.rows()[1][1] == "10");

    REQUIRE(alignment.columns.size() == 3);
    CHECK(alignment.columns[0] == TableAlignment::Kind::Left);
    CHECK(alignment.columns[1] == TableAlignment::Kind::Center);
    CHECK(alignment.columns[2] == TableAlignment::Kind::Right);
}

TEST_CASE("dsl/markdown: 首尾 | 可省且单元格裁剪空白") {
    const std::string source =
        "a | b | c\n"
        "--- | --- | ---\n"
        " 1 | 2 | 3 \n";
    const Result<document::TableModel> result = parse_markdown_table(source);
    REQUIRE(result.has_value());
    REQUIRE(result->header().size() == 3);
    CHECK(result->header()[0] == "a");
    CHECK(result->header()[2] == "c");
    REQUIRE(result->row_count() == 1);
    CHECK(result->rows()[0][0] == "1");
    CHECK(result->rows()[0][1] == "2");
    CHECK(result->rows()[0][2] == "3");
}

TEST_CASE("dsl/markdown: 转义的 \\| 是字面管道而非列分隔") {
    const std::string source =
        "| 表达式 | 含义 |\n"
        "| --- | --- |\n"
        "| a \\| b | 或 |\n";
    const Result<document::TableModel> result = parse_markdown_table(source);
    REQUIRE(result.has_value());
    REQUIRE(result->header().size() == 2);
    REQUIRE(result->row_count() == 1);
    CHECK(result->rows()[0].size() == 2);
    CHECK(result->rows()[0][0] == "a | b");
    CHECK(result->rows()[0][1] == "或");
}

TEST_CASE("dsl/markdown: 反引号代码段内的 | 不分列") {
    const std::string source =
        "| 代码 | 说明 |\n"
        "| --- | --- |\n"
        "| `a|b` | 位或 |\n";
    const Result<document::TableModel> result = parse_markdown_table(source);
    REQUIRE(result.has_value());
    REQUIRE(result->header().size() == 2);
    REQUIRE(result->row_count() == 1);
    CHECK(result->rows()[0][0] == "`a|b`");
    CHECK(result->rows()[0][1] == "位或");
}

TEST_CASE("dsl/markdown: 列数不一致时以表头为准（裁剪 / 补空）") {
    const std::string source =
        "| a | b |\n"
        "| --- | --- |\n"
        "| 1 | 2 | 3 |\n"
        "| 4 |\n";
    const Result<document::TableModel> result = parse_markdown_table(source);
    REQUIRE(result.has_value());
    REQUIRE(result->header().size() == 2);
    REQUIRE(result->row_count() == 2);

    CHECK(result->rows()[0].size() == 2);
    CHECK(result->rows()[0][0] == "1");
    CHECK(result->rows()[0][1] == "2");

    CHECK(result->rows()[1].size() == 2);
    CHECK(result->rows()[1][0] == "4");
    CHECK(result->rows()[1][1] == "");
}

TEST_CASE("dsl/markdown: 缺分隔行") {
    const std::string source = "| a | b |\n| 1 | 2 |\n";

    SUBCASE("默认要求分隔行 → ParseError") {
        ParseDiagnostic diagnostic;
        const Result<document::TableModel> result = parse_markdown_table(source, {}, &diagnostic);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::ParseError);
        CHECK_FALSE(diagnostic.message.empty());
        CHECK(diagnostic.line == 1);
    }
    SUBCASE("关掉 require_separator → 首行之后全部按数据行解析") {
        MarkdownTableOptions options;
        options.require_separator = false;
        const Result<document::TableModel> result = parse_markdown_table(source, options);
        REQUIRE(result.has_value());
        CHECK(result->header().size() == 2);
        REQUIRE(result->row_count() == 1);
        CHECK(result->rows()[0][0] == "1");
    }
}

TEST_CASE("dsl/markdown: 分隔行列数与表头不一致 → ParseError") {
    const Result<document::TableModel> result =
        parse_markdown_table("| a | b |\n| --- |\n| 1 | 2 |\n");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ParseError);
}

TEST_CASE("dsl/markdown: 非法分隔行单元格 → ParseError") {
    const Result<document::TableModel> result =
        parse_markdown_table("| a | b |\n| --- | x |\n| 1 | 2 |\n");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ParseError);
}

TEST_CASE("dsl/markdown: 空行结束表格") {
    const std::string source =
        "| a |\n"
        "| --- |\n"
        "| 1 |\n"
        "\n"
        "后面的段落不该被当成数据行 | x |\n";
    const Result<document::TableModel> result = parse_markdown_table(source);
    REQUIRE(result.has_value());
    REQUIRE(result->row_count() == 1);
    CHECK(result->rows()[0][0] == "1");
}

TEST_CASE("dsl/markdown: max_rows 超限 → OutOfRange") {
    std::string source = "| a |\n| --- |\n";
    for (int i = 0; i < 5; ++i) {
        source += "| v |\n";
    }
    MarkdownTableOptions options;
    options.max_rows = 3;
    const Result<document::TableModel> result = parse_markdown_table(source, options);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::OutOfRange);
}

TEST_CASE("dsl/markdown: 空表头输入 → ParseError 而非崩溃") {
    ParseDiagnostic diagnostic;
    const Result<document::TableModel> result = parse_markdown_table("", {}, &diagnostic);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ParseError);
    CHECK_FALSE(diagnostic.message.empty());
}

// ─────────────────────────────────────────────────────────────────────────
// CSV
// ─────────────────────────────────────────────────────────────────────────

TEST_CASE("dsl/csv: 基本解析与表头") {
    const std::string source = "name,age,city\nAlice,30,Beijing\nBob,25,Shanghai\n";
    const Result<document::TableModel> result = parse_csv(source);
    REQUIRE(result.has_value());
    REQUIRE(result->header().size() == 3);
    CHECK(result->header()[0] == "name");
    CHECK(result->header()[2] == "city");
    REQUIRE(result->row_count() == 2);
    CHECK(result->rows()[1][2] == "Shanghai");
}

TEST_CASE("dsl/csv: 引号字段含分隔符与换行") {
    const std::string source =
        "id,note\n"
        "1,\"a,b\"\n"
        "2,\"line1\nline2\"\n";
    const Result<document::TableModel> result = parse_csv(source);
    REQUIRE(result.has_value());
    REQUIRE(result->row_count() == 2);
    CHECK(result->rows()[0][1] == "a,b");
    CHECK(result->rows()[1][1] == "line1\nline2");
}

TEST_CASE("dsl/csv: 双引号转义 \"\"") {
    const std::string source = "id,q\n1,\"say \"\"hi\"\"\"\n";
    const Result<document::TableModel> result = parse_csv(source);
    REQUIRE(result.has_value());
    REQUIRE(result->row_count() == 1);
    CHECK(result->rows()[0][1] == "say \"hi\"");
}

TEST_CASE("dsl/csv: 引号内的空白原样保留") {
    const Result<document::TableModel> result = parse_csv("x\n\"  a  \"\n");
    REQUIRE(result.has_value());
    REQUIRE(result->row_count() == 1);
    CHECK(result->rows()[0][0] == "  a  ");
}

TEST_CASE("dsl/csv: 未闭合引号 → ParseError 且定位到起始引号") {
    const std::string source = "id,q\n1,\"abc\n";
    ParseDiagnostic diagnostic;
    const Result<document::TableModel> result = parse_csv(source, {}, &diagnostic);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ParseError);
    CHECK(diagnostic.line == 2);
    CHECK(diagnostic.column == 3);
}

TEST_CASE("dsl/csv: 跳过 UTF-8 BOM 且首个列名不带 BOM") {
    const std::string source = std::string("\xEF\xBB\xBF") + "name,age\nA,1\n";
    const Result<document::TableModel> result = parse_csv(source);
    REQUIRE(result.has_value());
    REQUIRE(result->header().size() == 2);
    CHECK(result->header()[0] == "name");
    CHECK(result->header()[0].size() == 4);
}

TEST_CASE("dsl/csv: 类型推断优先级与各类型") {
    const std::string source =
        "flag,num,price,when,label,blank\n"
        "true,42,3.14,2024-01-15,hello,\n"
        "false,7,2.0,2024-02-20,world,\n";
    const Result<std::vector<ColumnProfile>> profiles = sniff_columns(source);
    REQUIRE(profiles.has_value());
    REQUIRE(profiles->size() == 6);

    CHECK((*profiles)[0].name == "flag");
    CHECK((*profiles)[0].type == ColumnType::Boolean);
    CHECK((*profiles)[1].type == ColumnType::Integer);
    CHECK((*profiles)[2].type == ColumnType::Real);
    CHECK((*profiles)[3].type == ColumnType::Date);
    CHECK((*profiles)[4].type == ColumnType::Text);
    CHECK((*profiles)[5].type == ColumnType::Empty);

    CHECK((*profiles)[1].filled == 2);
    CHECK((*profiles)[1].empty == 0);
    CHECK((*profiles)[5].filled == 0);
    CHECK((*profiles)[5].empty == 2);
}

TEST_CASE("dsl/csv: Real 与 Text 的边界") {
    SUBCASE("科学计数法算 Real") {
        const Result<std::vector<ColumnProfile>> profiles = sniff_columns("x\n1e3\n2.5E-2\n");
        REQUIRE(profiles.has_value());
        CHECK((*profiles)[0].type == ColumnType::Real);
    }
    SUBCASE("多个小数点算 Text") {
        const Result<std::vector<ColumnProfile>> profiles = sniff_columns("x\n1.2.3\n");
        REQUIRE(profiles.has_value());
        CHECK((*profiles)[0].type == ColumnType::Text);
    }
    SUBCASE("残缺指数算 Text") {
        const Result<std::vector<ColumnProfile>> profiles = sniff_columns("x\n1e\n");
        REQUIRE(profiles.has_value());
        CHECK((*profiles)[0].type == ColumnType::Text);
    }
    SUBCASE("纯数字算 Integer 而非 Real") {
        const Result<std::vector<ColumnProfile>> profiles = sniff_columns("x\n1\n2\n");
        REQUIRE(profiles.has_value());
        CHECK((*profiles)[0].type == ColumnType::Integer);
    }
    SUBCASE("0/1 优先判为 Boolean") {
        const Result<std::vector<ColumnProfile>> profiles = sniff_columns("x\n0\n1\n");
        REQUIRE(profiles.has_value());
        CHECK((*profiles)[0].type == ColumnType::Boolean);
    }
    SUBCASE("日期带时间算 Date") {
        const Result<std::vector<ColumnProfile>> profiles =
            sniff_columns("x\n2024-01-15 08:30:00\n2024-02-20T09:00\n");
        REQUIRE(profiles.has_value());
        CHECK((*profiles)[0].type == ColumnType::Date);
    }
}

TEST_CASE("dsl/csv: sniff_rows 限定样本") {
    const std::string source = "x\n2\n3\n4\nabc\n";

    SUBCASE("sniff_rows=0 视为全部行") {
        CsvOptions options;
        options.sniff_rows = 0;
        const Result<std::vector<ColumnProfile>> profiles = sniff_columns(source, options);
        REQUIRE(profiles.has_value());
        CHECK((*profiles)[0].type == ColumnType::Text);
        CHECK((*profiles)[0].filled == 4);
    }
    SUBCASE("sniff_rows=2 只看前两行") {
        CsvOptions options;
        options.sniff_rows = 2;
        const Result<std::vector<ColumnProfile>> profiles = sniff_columns(source, options);
        REQUIRE(profiles.has_value());
        CHECK((*profiles)[0].type == ColumnType::Integer);
        CHECK((*profiles)[0].filled == 2);
    }
}

TEST_CASE("dsl/csv: has_header=false 用 col1..colN") {
    const std::string source = "1,2,3\n4,5,6\n";

    SUBCASE("sniff_columns 生成列名") {
        CsvOptions options;
        options.has_header = false;
        const Result<std::vector<ColumnProfile>> profiles = sniff_columns(source, options);
        REQUIRE(profiles.has_value());
        REQUIRE(profiles->size() == 3);
        CHECK((*profiles)[0].name == "col1");
        CHECK((*profiles)[2].name == "col3");
        CHECK((*profiles)[0].type == ColumnType::Integer);
        CHECK((*profiles)[0].filled == 2);
    }
    SUBCASE("parse_csv 不设表头且保留全部行") {
        CsvOptions options;
        options.has_header = false;
        const Result<document::TableModel> result = parse_csv("a,b\nc,d\n", options);
        REQUIRE(result.has_value());
        CHECK(result->header().empty());
        CHECK(result->row_count() == 2);
    }
}

TEST_CASE("dsl/csv: 自定义分隔符") {
    const std::string source = "name;age\nA;1\n";
    CsvOptions options;
    options.delimiter = ';';
    const Result<document::TableModel> result = parse_csv(source, options);
    REQUIRE(result.has_value());
    REQUIRE(result->header().size() == 2);
    CHECK(result->header()[1] == "age");
    CHECK(result->rows()[0][1] == "1");
}

TEST_CASE("dsl/csv: CRLF 行尾") {
    const std::string source = "a,b\r\n1,2\r\n";
    const Result<document::TableModel> result = parse_csv(source);
    REQUIRE(result.has_value());
    REQUIRE(result->row_count() == 1);
    CHECK(result->rows()[0][1] == "2");
}

TEST_CASE("dsl/csv: max_rows 超限 → OutOfRange") {
    std::string source = "x\n";
    for (int i = 0; i < 5; ++i) {
        source += "v\n";
    }
    CsvOptions options;
    options.max_rows = 3;
    const Result<document::TableModel> result = parse_csv(source, options);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::OutOfRange);
}

TEST_CASE("dsl/csv: 非法分隔符 → InvalidArgument") {
    CsvOptions options;
    options.delimiter = '"';
    const Result<document::TableModel> result = parse_csv("a,b\n", options);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvalidArgument);
}

TEST_CASE("dsl/csv: 空白行被跳过") {
    const std::string source = "a,b\n\n1,2\n\n\n3,4\n";
    const Result<document::TableModel> result = parse_csv(source);
    REQUIRE(result.has_value());
    REQUIRE(result->row_count() == 2);
    CHECK(result->rows()[0][0] == "1");
    CHECK(result->rows()[1][0] == "3");
}

}  // namespace
