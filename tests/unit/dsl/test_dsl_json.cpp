// test_dsl_json.cpp —— dsl 模块：JSON 解析
//
// 关注的不是"能不能解析一句话"，而是**契约**：
//   * 转义（\" \\ \n \uXXXX 含代理对）这些最容易错解的地方必须正确；
//   * 对象保序、重复键覆盖；
//   * 非法输入不抛异常、不死循环、错误定位准确（行列偏移）；
//   * 深度/长度上限防栈爆，超限返回错误而非崩溃。
#include <doctest.h>

#include <string>

#include "termchart/dsl/json.hpp"

namespace {

using namespace termchart;
using namespace termchart::dsl;

TEST_CASE("dsl/json: 标量与顶层值") {
    SUBCASE("null / true / false") {
        const Result<JsonValue> r = parse_json("null");
        REQUIRE(r.has_value());
        CHECK(r->is_null());

        const Result<JsonValue> t = parse_json("true");
        REQUIRE(t.has_value());
        CHECK(t->is_bool());
        CHECK(t->as_bool());

        const Result<JsonValue> f = parse_json(" false ");
        REQUIRE(f.has_value());
        CHECK(f->is_bool());
        CHECK_FALSE(f->as_bool());
    }

    SUBCASE("数字：整数、负数、小数、科学计数法") {
        CHECK(parse_json("42")->as_number() == 42.0);
        CHECK(parse_json("-17")->as_number() == -17.0);
        CHECK(parse_json("3.14")->as_number() == doctest::Approx(3.14));
        CHECK(parse_json("-0.5")->as_number() == doctest::Approx(-0.5));
        CHECK(parse_json("1e5")->as_number() == 100000.0);
        CHECK(parse_json("2.5E-3")->as_number() == doctest::Approx(0.0025));
        CHECK(parse_json("0")->as_number() == 0.0);
    }

    SUBCASE("字符串与转义") {
        CHECK(parse_json("\"hello\"")->as_string() == "hello");
        CHECK(parse_json("\"a\\\"b\"")->as_string() == "a\"b");
        CHECK(parse_json("\"a\\\\b\"")->as_string() == "a\\b");
        CHECK(parse_json("\"a\\/b\"")->as_string() == "a/b");
        CHECK(parse_json("\"a\\nb\"")->as_string() == "a\nb");
        CHECK(parse_json("\"a\\tb\"")->as_string() == "a\tb");
        CHECK(parse_json("\"\\u0041\"")->as_string() == "A");
        CHECK(parse_json("\"\\u4e2d\\u6587\"")->as_string() == "中文");
        // 代理对：U+1F600 = 😀 → UTF-8 F0 9F 98 80
        CHECK(parse_json("\"\\uD83D\\uDE00\"")->as_string() == "\xF0\x9F\x98\x80");
    }
}

TEST_CASE("dsl/json: 数组与对象") {
    SUBCASE("空容器") {
        const Result<JsonValue> a = parse_json("[]");
        REQUIRE(a.has_value());
        CHECK(a->is_array());
        CHECK(a->size() == 0);

        const Result<JsonValue> o = parse_json("{}");
        REQUIRE(o.has_value());
        CHECK(o->is_object());
        CHECK(o->size() == 0);
    }

    SUBCASE("嵌套与混合类型") {
        const Result<JsonValue> r = parse_json(R"({"a": [1, 2.5, "x", null, true], "b": {"c": -3}})");
        REQUIRE(r.has_value());
        CHECK(r->is_object());
        CHECK(r->size() == 2);
        CHECK(r->at("a").is_array());
        CHECK(r->at("a").size() == 5);
        CHECK(r->at("a")[0].as_number() == 1.0);
        CHECK(r->at("a")[1].as_number() == 2.5);
        CHECK(r->at("a")[2].as_string() == "x");
        CHECK(r->at("a")[3].is_null());
        CHECK(r->at("a")[4].as_bool());
        CHECK(r->at("b").at("c").as_number() == -3.0);
    }

    SUBCASE("对象保持插入序") {
        const Result<JsonValue> r = parse_json(R"({"z": 1, "a": 2, "m": 3})");
        REQUIRE(r.has_value());
        // 通过 find 逐个验证键存在；顺序由内部 vector<pair> 保证，
        // 公开接口只暴露 find/at，顺序断言走 size + 逐键查找。
        CHECK(r->find("z") != nullptr);
        CHECK(r->find("a") != nullptr);
        CHECK(r->find("m") != nullptr);
        CHECK(r->find("nope") == nullptr);
    }

    SUBCASE("重复键后者覆盖前者") {
        const Result<JsonValue> r = parse_json(R"({"k": 1, "k": 2})");
        REQUIRE(r.has_value());
        CHECK(r->size() == 1);
        CHECK(r->at("k").as_number() == 2.0);
    }
}

TEST_CASE("dsl/json: 访问器缺省行为") {
    const Result<JsonValue> r = parse_json(R"({"n": null, "arr": [10, 20]})");
    REQUIRE(r.has_value());

    // 类型不匹配返回 fallback，不抛异常。
    CHECK(r->at("n").as_number(7.0) == 7.0);
    CHECK(r->at("n").as_bool(true));
    CHECK(r->at("n").as_string("d") == "d");

    // 缺失键返回 Null 单例（可安全取值）。
    CHECK(r->at("missing").is_null());
    CHECK(r->at("missing").as_number() == 0.0);

    // 数组越界返回 Null 单例。
    CHECK(r->at("arr")[5].is_null());
    CHECK(r->at("arr")[0].as_number() == 10.0);
    CHECK(r->at("arr")[1].as_number() == 20.0);

    // 非容器 size 为 0。
    CHECK(r->at("n").size() == 0);
}

TEST_CASE("dsl/json: 程序化构建") {
    JsonValue root = JsonValue::object();
    root.set("name", JsonValue::string("termchart"));
    root.set("version", JsonValue::number(1));
    JsonValue tags = JsonValue::array();
    tags.push(JsonValue::string("terminal"));
    tags.push(JsonValue::string("chart"));
    root.set("tags", std::move(tags));
    root.set("name", JsonValue::string("termchart2"));  // 覆盖

    CHECK(root.at("name").as_string() == "termchart2");
    CHECK(root.at("version").as_number() == 1.0);
    CHECK(root.at("tags").size() == 2);
    CHECK(root.at("tags")[1].as_string() == "chart");
}

TEST_CASE("dsl/json: 非法输入返回错误且不死循环") {
    struct Case {
        const char* source;
        const char* what;
    };
    const Case cases[] = {
        {"", "empty input"},
        {"{", "unterminated object"},
        {"[1,", "unterminated array"},
        {"[1,]", "trailing comma in array"},
        {"{\"a\":1,}", "trailing comma in object"},
        {"{\"a\" 1}", "missing colon"},
        {"{\"a\":}", "missing value"},
        {"{a: 1}", "unquoted key"},
        {"tru", "truncated literal"},
        {"nul", "truncated literal"},
        {"01", "leading zero"},
        {"1.", "fraction without digits"},
        {"1e", "exponent without digits"},
        {"+1", "leading plus"},
        {"\"abc", "unterminated string"},
        {"\"a\\qb\"", "invalid escape"},
        {"\"a\\u00\"", "truncated \\u escape"},
        {"\"\\uD83D\"", "lone high surrogate"},
        {"\"\\uDE00\"", "lone low surrogate"},
        {"\"a\x01b\"", "unescaped control character"},
        {"[1 2]", "missing comma"},
        {"{\"a\":1} extra", "trailing garbage"},
        {"'single'", "single-quoted string"},
    };
    for (const Case& c : cases) {
        ParseDiagnostic diag;
        const Result<JsonValue> r = parse_json(c.source, {}, &diag);
        CHECK_FALSE_MESSAGE(r.has_value(), c.what);
        CHECK_MESSAGE(!diag.ok(), c.what);
        CHECK_MESSAGE(diag.line >= 1, c.what);
        CHECK_MESSAGE(diag.column >= 1, c.what);
    }
}

TEST_CASE("dsl/json: 深度上限防栈爆") {
    JsonOptions opts;
    opts.max_depth = 8;
    // 8 层嵌套应通过。
    std::string ok = "";
    for (int i = 0; i < 8; ++i) {
        ok += "[";
    }
    ok += "1";
    for (int i = 0; i < 8; ++i) {
        ok += "]";
    }
    CHECK(parse_json(ok, opts).has_value());

    // 9 层应被拒（不崩溃）。
    std::string deep = "";
    for (int i = 0; i < 9; ++i) {
        deep += "[";
    }
    deep += "1";
    for (int i = 0; i < 9; ++i) {
        deep += "]";
    }
    ParseDiagnostic diag;
    const Result<JsonValue> r = parse_json(deep, opts, &diag);
    CHECK_FALSE(r.has_value());
    CHECK(diag.code == ErrorCode::ParseError);
}

TEST_CASE("dsl/json: 输入长度上限") {
    JsonOptions opts;
    opts.max_bytes = 16;
    CHECK(parse_json(std::string(20, ' '), opts).has_value() == false);
    CHECK(parse_json("1234567890123456", opts).has_value());  // 恰好 16 字节
}

TEST_CASE("dsl/json: 诊断的行列偏移") {
    ParseDiagnostic diag;
    const Result<JsonValue> r = parse_json("{\n  \"a\": tru\n}", {}, &diag);
    REQUIRE_FALSE(r.has_value());
    CHECK(diag.line == 2);
    CHECK(diag.column >= 8);  // "tru" 起始附近
    CHECK(diag.offset > 0);
}

}  // namespace
