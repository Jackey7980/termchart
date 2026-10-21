// tests/unit/data/test_scalar_parse.cpp —— 脏标量解析（"赛博法医"入口）测试
//
// doctest：doctest.h vendored 于 third_party/。样本全部内置，无外部 fixture。
#include "termchart/data/scalar_parse.hpp"

#include <cmath>

#include <doctest.h>

using namespace termchart::data;

namespace {
bool is_nan(double d) { return std::isnan(d); }
}  // namespace

TEST_CASE("scalar_parse::plain numbers") {
    CHECK(parse_scalar("123").value == 123.0);
    CHECK(parse_scalar("  42.5 ").value == 42.5);      // 前后空白
    CHECK(parse_scalar("-7").value == -7.0);
    CHECK(parse_scalar("1e3").value == 1000.0);        // 科学计数
    CHECK(parse_scalar("+5").value == 5.0);            // 前导 +
}

TEST_CASE("scalar_parse::thousands separators") {
    CHECK(parse_scalar("1,234.50").value == 1234.5);
    CHECK(parse_scalar("1,000,000").value == 1000000.0);
}

TEST_CASE("scalar_parse::currency symbols") {
    CHECK(parse_scalar("$1,234.50").value == 1234.5);
    CHECK(parse_scalar("¥999").value == 999.0);
    CHECK(parse_scalar("€42").value == 42.0);
    CHECK(parse_scalar("£10").value == 10.0);
    CHECK(parse_scalar("RMB100").value == 100.0);
    CHECK(parse_scalar("USD 50").value == 50.0);
    CHECK(parse_scalar("12.5元").value == 12.5);        // 中文后缀
}

TEST_CASE("scalar_parse::percent") {
    CHECK(parse_scalar("15%").value == 0.15);
    CHECK(parse_scalar("100%").value == 1.0);
    CHECK(parse_scalar("  15 % ").value == 0.15);       // 中间空白也容忍
}

TEST_CASE("scalar_parse::accounting parentheses (negative)") {
    CHECK(parse_scalar("(123.45)").value == -123.45);
    CHECK(parse_scalar("($1,000.00)").value == -1000.0);
}

TEST_CASE("scalar_parse::unicode minus U+2212") {
    // "−123" 用 U+2212（并非 ASCII 连字符）
    CHECK(parse_scalar("−123").value == -123.0);
}

TEST_CASE("scalar_parse::missing tokens -> Empty / NaN") {
    auto na = parse_scalar("NA");
    CHECK(na.missing());
    CHECK(is_nan(na.value));

    CHECK(parse_scalar("N/A").missing());
    CHECK(parse_scalar("").missing());
    CHECK(parse_scalar("   ").missing());
    CHECK(parse_scalar("null").missing());
    CHECK(parse_scalar("NULL").missing());
    CHECK(parse_scalar("NaN").missing());
    CHECK(parse_scalar("-").missing());
    CHECK(parse_scalar("--").missing());
    CHECK(parse_scalar("—").missing());  // em dash

    CHECK(is_missing_token("na"));
    CHECK(is_missing_token("n/a"));
    CHECK_FALSE(is_missing_token("nab"));
}

TEST_CASE("scalar_parse::non-numeric -> NotNumeric") {
    auto r = parse_scalar("abc");
    CHECK(r.status == ScalarParseStatus::NotNumeric);
    CHECK(is_nan(r.value));

    // 尾部杂质（严格 from_chars）：不能只认前缀。
    CHECK(parse_scalar("12px").status == ScalarParseStatus::NotNumeric);
    CHECK(parse_scalar("1.2.3").status == ScalarParseStatus::NotNumeric);
}

TEST_CASE("scalar_parse::composed (currency + percent + parentheses)") {
    // 宽松组合：$50% 这类虽然罕见也照常解析（÷100 后符号保留）。
    CHECK(parse_scalar("$50%").value == 0.5);
}
