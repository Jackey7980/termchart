// test_io.cpp —— io 模块单测（csv / json 收编 / stream）
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <doctest.h>

#include "termchart/core/error.hpp"
#include "termchart/io/csv.hpp"
#include "termchart/io/json.hpp"
#include "termchart/io/stream.hpp"

// dsl shim：验证收编后 dsl:: 命名空间入口仍可用（using 别名）。
#include "termchart/dsl/json.hpp"

using namespace termchart::io;

namespace {

bool approx(double a, double b, double eps = 1e-9) { return std::fabs(a - b) < eps; }

}  // namespace

TEST_SUITE("io") {

// ── csv ────────────────────────────────────────────────────────────────────

TEST_CASE("io::csv::基本数值表") {
    const std::string text = "x,y\n1,2\n3,4\n5,6\n";
    const auto r = parse_csv(text);
    REQUIRE(r.has_value());
    const CsvTable& t = r.value();
    CHECK(t.row_count == 3);
    REQUIRE(t.headers.size() == 2);
    CHECK(t.headers[0] == "x");
    CHECK(t.headers[1] == "y");
    REQUIRE(t.columns.size() == 2);
    CHECK(t.columns[0].size() == 3);
    CHECK(t.columns[0][0] == 1.0);
    CHECK(t.columns[1][2] == 6.0);
}

TEST_CASE("io::csv::无表头 col0..colN") {
    const auto r = parse_csv("1,2\n3,4\n", CsvOptions{',', false, 0});
    REQUIRE(r.has_value());
    const CsvTable& t = r.value();
    CHECK(t.headers.size() == 2);
    CHECK(t.headers[0] == "col0");
    CHECK(t.headers[1] == "col1");
    CHECK(t.row_count == 2);
}

TEST_CASE("io::csv::引号字段与转义") {
    // 引号包裹的数字按数值解析（宽松口径）；"" 转义出一个字面引号
    const std::string text = "a,b\n\"12\",\"3.5\"\n\"7\",\"9\"\n";
    const auto r = parse_csv(text, CsvOptions{',', true, 0});
    REQUIRE(r.has_value());
    const CsvTable& t = r.value();
    CHECK(t.columns[0][0] == 12.0);
    CHECK(t.columns[0][1] == 7.0);
    CHECK(t.columns[1][0] == 3.5);
    // 引号内含非数字 → ParseError（数值入口口径）
    ParseDiagnostic diag;
    const auto bad = parse_csv("a,b\n\"x\",\"1\"\n", CsvOptions{',', true, 0}, &diag);
    CHECK_FALSE(bad.has_value());
    // 未闭合引号 → ParseError
    const auto unclosed = parse_csv("a,b\n\"x,1\n", CsvOptions{',', true, 0}, &diag);
    CHECK_FALSE(unclosed.has_value());
    CHECK(diag.code == termchart::ErrorCode::ParseError);
}

TEST_CASE("io::csv::缺值口径：空与 NA → NaN") {
    const std::string text = "a,b\n1,\n,2\nNA,3\n";
    const auto r = parse_csv(text, CsvOptions{',', true, 0});
    REQUIRE(r.has_value());
    const CsvTable& t = r.value();
    CHECK(std::isnan(t.columns[0][0]) == false);   // 1
    CHECK(std::isnan(t.columns[1][0]));            // 空
    CHECK(std::isnan(t.columns[0][1]));            // 空
    CHECK(std::isnan(t.columns[0][2]));            // NA
    CHECK(t.columns[1][2] == 3.0);
}

TEST_CASE("io::csv::注释行 / CRLF / BOM") {
    const std::string text = "\xEF\xBB\xBF# leading comment\r\nv\r\n1.5\r\n2.5\r\n";
    const auto r = parse_csv(text, CsvOptions{',', true, '#'});
    REQUIRE(r.has_value());
    const CsvTable& t = r.value();
    REQUIRE(t.columns.size() == 1);
    CHECK(t.columns[0].size() == 2);
    CHECK(t.columns[0][0] == 1.5);
    CHECK(t.columns[0][1] == 2.5);
}

TEST_CASE("io::csv::行列不一致与非数字均报诊断") {
    ParseDiagnostic diag;
    const auto bad = parse_csv("a,b\n1,2,3\n", CsvOptions{',', true, 0}, &diag);
    CHECK_FALSE(bad.has_value());
    CHECK(diag.line >= 2);
    CHECK_FALSE(diag.ok());
    const auto txt = parse_csv("a\nhello\n", CsvOptions{',', true, 0}, &diag);
    CHECK_FALSE(txt.has_value());
    CHECK(diag.code == termchart::ErrorCode::ParseError);
    CHECK(diag.column == 1);
}

TEST_CASE("io::csv::delimiter 自动嗅探（分号 / tab）") {
    const auto semi = parse_csv("a;b\n1;2\n");
    REQUIRE(semi.has_value());
    CHECK(semi.value().columns.size() == 2);
    const auto tab = parse_csv("a\tb\n1\t2\n");
    REQUIRE(tab.has_value());
    CHECK(tab.value().columns.size() == 2);
}

TEST_CASE("io::csv::空输入合法") {
    const auto r = parse_csv("");
    REQUIRE(r.has_value());
    CHECK(r.value().row_count == 0);
    CHECK(r.value().columns.empty());
}

// ── json（收编验证：io 直链 + dsl shim 别名）───────────────────────────────

TEST_CASE("io::json::解析与取值（含 dsl shim 别名）") {
    const std::string text = R"({"n": 42, "s": "a\"b", "arr": [1, 2.5, true], "nested": {"k": null}})";
    const auto r = parse_json(text);
    REQUIRE(r.has_value());
    const JsonValue& v = r.value();
    CHECK(v.find("n")->as_number() == 42.0);
    CHECK(v.find("s")->as_string() == "a\"b");
    CHECK(v.find("arr")->size() == 3);
    CHECK(v.find("arr")->at(0).as_number() == 1.0);
    CHECK(v.find("arr")->at(1).as_number() == 2.5);
    CHECK(v.find("arr")->at(2).as_bool());
    CHECK(v.find("nested")->find("k")->is_null());
    CHECK(v.find("missing") == nullptr);

    // dsl shim：同一实现，dsl 命名空间别名必须可用
    const auto r2 = termchart::dsl::parse_json(text);
    REQUIRE(r2.has_value());
    CHECK(r2.value().find("n")->as_number() == 42.0);
}

TEST_CASE("io::json::错误码与诊断") {
    ParseDiagnostic diag;
    const auto bad = parse_json("{\"a\": }", {}, &diag);
    CHECK_FALSE(bad.has_value());
    CHECK(diag.code == termchart::ErrorCode::ParseError);
    CHECK(diag.line >= 1);
    // 深度上限 fail-loud
    std::string deep = "[";
    for (int i = 0; i < 100; ++i) deep += "[";
    deep += "]";
    for (int i = 0; i < 100; ++i) deep += "]";
    const auto too_deep = parse_json(deep);
    CHECK_FALSE(too_deep.has_value());
}

// ── stream ─────────────────────────────────────────────────────────────────

TEST_CASE("io::stream::内存源多行与 CRLF") {
    auto reader = LineReader::from_string("l1\r\nl2\nl3");
    CHECK(reader.valid());
    std::string_view line;
    REQUIRE(reader.next(line));
    CHECK(line == "l1");
    REQUIRE(reader.next(line));
    CHECK(line == "l2");
    REQUIRE(reader.next(line));
    CHECK(line == "l3");
    CHECK_FALSE(reader.next(line));
    // 空串也是合法源（doctest 宏按 const 引用捕获表达式，须用具名非 const 对象）
    auto empty_reader = LineReader::from_string("");
    CHECK(empty_reader.valid());
    std::string_view no_line;
    CHECK_FALSE(empty_reader.next(no_line));
}

TEST_CASE("io::stream::超长行跨块不 OOM") {
    // 200KB 单行，缓冲 64KB 起，必走 compact + 翻倍路径
    std::string big(200000, 'x');
    big += "\ntail\n";
    auto reader = LineReader::from_string(std::move(big));
    std::string_view line;
    REQUIRE(reader.next(line));
    CHECK(line.size() == 200000);
    REQUIRE(reader.next(line));
    CHECK(line == "tail");
}

TEST_CASE("io::stream::文件源 open/next/close") {
    // 临时文件走系统 temp 目录：不假设 ctest 的工作目录（ctest 在 build 目录跑，
    // 仓库相对路径会写不进）。
    const std::string path =
        (std::filesystem::temp_directory_path() / "termchart_test_io_stream.csv").string();
    {
        std::ofstream out(path, std::ios::binary);
        out << "h1,h2\n1,2\n3,4\n";
    }
    auto r = LineReader::open(path);
    REQUIRE(r.has_value());
    // Result::value() 返回左值引用，std::move(r).value() 仍会调拷贝；
    // 必须 std::move(*r) 拿到 T&& 走移动构造（LineReader 拷贝已 delete）。
    LineReader reader = std::move(*r);
    CHECK(reader.valid());
    std::string_view line;
    REQUIRE(reader.next(line));
    CHECK(line == "h1,h2");
    REQUIRE(reader.next(line));
    CHECK(line == "1,2");
    REQUIRE(reader.next(line));
    CHECK(line == "3,4");
    CHECK_FALSE(reader.next(line));
    reader.close();
    CHECK_FALSE(reader.valid());
    std::remove(path.c_str());
    // 不存在的文件 fail-loud
    const auto missing = LineReader::open("build/ninja/_definitely_missing_io.csv");
    CHECK_FALSE(missing.has_value());
}

}  // TEST_SUITE
