// tests/unit/data/test_dataframe.cpp —— 容错 CSV 读取（"法医级"宽容读取器）测试
//
// doctest：doctest.h vendored 于 third_party/。样本全部内置，无外部 fixture。
// 核心断言：文本列不报错、脏值清洗成数、缺列补 Ragged、整表绝不抛/绝不返回错误。
#include "termchart/data/dataframe.hpp"

#include <doctest.h>

using namespace termchart::data;

TEST_CASE("dataframe::clean numeric table") {
    const char* csv =
        "date,value\n"
        "2026-01-01,10\n"
        "2026-01-02,20\n"
        "2026-01-03,30\n";
    const DataFrame f = parse_dataframe(csv);
    CHECK(f.row_count == 3);
    CHECK(f.headers.size() == 2);
    CHECK(f.columns.size() == 2);
    CHECK(f.columns[1][0].is_numeric());
    CHECK(f.columns[1][0].num == 10.0);
    CHECK(f.columns[1][2].num == 30.0);
}

TEST_CASE("dataframe::text column preserved (not error)") {
    // Region 列是文本，io/csv 会 ParseError，这里应当保留为 Text 原文。
    const char* csv =
        "region,sales\n"
        "华东,100\n"
        "华北,200\n";
    const DataFrame f = parse_dataframe(csv);
    CHECK(f.row_count == 2);
    CHECK(f.columns[0][0].is_text());
    CHECK(f.columns[0][0].raw == "华东");
    CHECK(f.columns[1][1].is_numeric());
    CHECK(f.columns[1][1].num == 200.0);
}

TEST_CASE("dataframe::dirty values cleaned with flags") {
    // 注意：千分位逗号若未加引号，会被 CSV 分隔符切分——这里显式给 "1,234.50" 加引号，
    // 与标准 CSV 一致；scalar_parse 在"单字段到达"时（JSON/引号单元格/单列）仍能认出千分位。
    const char* csv =
        "amount\n"
        "\"1,234.50\"\n"  // 千分位（引号保护）
        "(500.00)\n"      // 会计负
        "15%\n"           // 百分号
        "¥999\n";         // 货币
    const DataFrame f = parse_dataframe(csv);
    CHECK(f.row_count == 4);
    CHECK(f.columns[0][0].is_numeric());
    CHECK(f.columns[0][0].num == 1234.5);
    CHECK(has_cell_flag(f.columns[0][0].flags, CellFlag::HasComma));
    CHECK(f.columns[0][1].num == -500.0);
    CHECK(has_cell_flag(f.columns[0][1].flags, CellFlag::AccountingNegative));
    CHECK(f.columns[0][2].num == 0.15);
    CHECK(has_cell_flag(f.columns[0][2].flags, CellFlag::Percent));
    CHECK(f.columns[0][3].num == 999.0);
    CHECK(has_cell_flag(f.columns[0][3].flags, CellFlag::Currency));
}

TEST_CASE("dataframe::ragged rows tolerated (no rectangle enforcement)") {
    // 第 2 行少一列，第 3 行多一列——io/csv 会 InvalidArgument，这里补 Ragged 空位。
    // 数据行索引：r0="1,2"（2 列），r1="3,4,5,6"（4 列）。
    const char* csv =
        "a,b,c\n"
        "1,2\n"
        "3,4,5,6\n";
    const DataFrame f = parse_dataframe(csv);
    CHECK(f.row_count == 2);
    CHECK(f.columns.size() == 4);  // 列数取最大宽度
    // r0 缺第 3、4 列 → Ragged 空位
    CHECK(f.columns[2][0].is_missing());
    CHECK(has_cell_flag(f.columns[2][0].flags, CellFlag::Ragged));
    CHECK(f.columns[3][0].is_missing());
    CHECK(has_cell_flag(f.columns[3][0].flags, CellFlag::Ragged));
    // r1 列齐全（第 4 列值为 6）
    CHECK(f.columns[3][1].is_numeric());
    CHECK(f.columns[3][1].num == 6.0);
    // r1 不再缺列，第 4 列是数字而非空位
    CHECK_FALSE(f.columns[3][1].is_missing());
}

TEST_CASE("dataframe::missing cells flagged") {
    const char* csv =
        "x,y\n"
        "1,\n"
        "NA,2\n";
    const DataFrame f = parse_dataframe(csv);
    CHECK(f.row_count == 2);
    CHECK(f.columns[1][0].is_missing());  // 空字段
    CHECK(f.columns[0][1].is_missing());  // NA
    CHECK(f.columns[1][1].is_numeric());
    CHECK(f.columns[1][1].num == 2.0);
}

TEST_CASE("dataframe::quoted field with delimiter/comma") {
    const char* csv =
        "label,value\n"
        "\"east,west\",\"1,234.50\"\n";  // 引号内含逗号 + 引号保护千分位值
    const DataFrame f = parse_dataframe(csv);
    CHECK(f.row_count == 1);
    CHECK(f.columns[0][0].is_text());
    CHECK(f.columns[0][0].raw == "east,west");
    CHECK(f.columns[1][0].is_numeric());
    CHECK(f.columns[1][0].num == 1234.5);
}
