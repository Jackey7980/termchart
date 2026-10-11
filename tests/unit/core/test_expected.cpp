// test_expected.cpp —— Expected<T, E> 语义
#include <doctest.h>

#include <stdexcept>
#include <string>

#include "termchart/core/expected.hpp"
#include "termchart/core/error.hpp"

namespace {

using namespace termchart;

TEST_CASE("core: Result 成功路径") {
    const Result<int> ok = 42;
    REQUIRE(ok.has_value());
    CHECK(ok.value() == 42);
    CHECK(*ok == 42);
    CHECK(ok.value_or(0) == 42);
}

TEST_CASE("core: Result 失败路径") {
    const Result<int> bad = Unexpected<ErrorCode>(ErrorCode::OutOfRange);
    CHECK_FALSE(bad.has_value());
    CHECK(bad.error() == ErrorCode::OutOfRange);
    CHECK(bad.value_or(7) == 7);
}

TEST_CASE("core: 无值访问 value() 抛异常") {
    const Result<int> bad = Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    CHECK_THROWS_AS((void)bad.value(), std::logic_error);

    const Result<int> ok = 1;
    CHECK_THROWS_AS((void)ok.error(), std::logic_error);
}

TEST_CASE("core: and_then 短路传递错误") {
    const auto double_it = [](const int& v) -> Result<int> { return v * 2; };
    const Result<int> ok = 21;
    CHECK(ok.and_then(double_it).value() == 42);

    const Result<int> bad = Unexpected<ErrorCode>(ErrorCode::Unsupported);
    const Result<int> chained = bad.and_then(double_it);
    CHECK_FALSE(chained.has_value());
    CHECK(chained.error() == ErrorCode::Unsupported);
}

TEST_CASE("core: 错误码分类与标识稳定") {
    CHECK(category_of(ErrorCode::Ok) == "none");
    CHECK(category_of(ErrorCode::OutOfRange) == "logic");
    CHECK(category_of(ErrorCode::Unsupported) == "domain");
    CHECK(category_of(ErrorCode::MalformedUtf8) == "encoding");
    CHECK(category_of(ErrorCode::IoError) == "platform");
    CHECK(std::string(to_string(ErrorCode::NotInitialized)) == "NotInitialized");
    CHECK(is_ok(ErrorCode::Ok));
}

}  // namespace
