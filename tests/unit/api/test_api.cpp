// test_api.cpp —— api 层（Opts / 渲染入口 / 输出封装）
//
// api 是唯一允许碰全部模块的层，也是 GUI 平行栈将来要对齐的"对外契约面"。
// 关键契约三条：
//   1. Opts 每项都有默认值，零配置可用；
//   2. render_line **只产出字节，不写 stdout**（headless / 测试 / GUI 内嵌都依赖）；
//   3. 参数非法返回 Result 错误，不抛异常。
#include <doctest.h>

#include <cstdio>
#include <optional>
#include <string>

#include "termchart/api/opts.hpp"
#include "termchart/api/print.hpp"
#include "termchart/api/quick.hpp"
#include "termchart/core/error.hpp"
#include "termchart/core/style.hpp"

namespace {

using namespace termchart;
using namespace termchart::api;

}  // namespace

TEST_CASE("api: Opts 默认全空且链式可写") {
    const Opts defaults;
    CHECK_FALSE(defaults.width().has_value());
    CHECK_FALSE(defaults.height().has_value());
    CHECK_FALSE(defaults.color_depth().has_value());
    CHECK(defaults.unicode());
    CHECK(defaults.newline());
    CHECK(defaults.reset_at_end());

    Opts opts;
    opts.width(80).height(24).color_depth(ColorDepth::Ansi256).ascii_only().newline(false);
    CHECK(opts.width() == std::optional<std::uint16_t>(80));
    CHECK(opts.height() == std::optional<std::uint16_t>(24));
    CHECK(opts.color_depth() == ColorDepth::Ansi256);
    CHECK_FALSE(opts.unicode());
    CHECK_FALSE(opts.newline());
}

TEST_CASE("api: render_line 一行文本 → 带样式的字节流") {
    const Result<std::string> out = render_line("hi", Style{}.set_fg(Color::rgb(1, 2, 3)));
    REQUIRE(out.has_value());
    CHECK(out->find("hi") != std::string::npos);
    CHECK(out->find("\x1b[") != std::string::npos);
    CHECK(out->back() == '\n');  // 默认补换行
}

TEST_CASE("api: 显式宽度 0 是参数错误，不猜也不钳位") {
    Opts zero;
    zero.width(0);
    const Result<std::string> out = render_line("hi", Style{}, zero);
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error() == ErrorCode::InvalidArgument);
}

TEST_CASE("api: 宽度不足时截断而不是抛异常") {
    Opts narrow;
    narrow.width(2);
    const Result<std::string> out = render_line("hello", Style{}, narrow);
    REQUIRE(out.has_value());
    CHECK(out->find("he") != std::string::npos);
    CHECK(out->find("hello") == std::string::npos);
}

TEST_CASE("api: 空文本退化成一格而不是报错") {
    const Result<std::string> out = render_line("", Style{});
    REQUIRE(out.has_value());
    CHECK_FALSE(out->empty());
}

TEST_CASE("api: 深度降级由 render/sgr 统一接管") {
    Opts opts;
    opts.color_depth(ColorDepth::Ansi16);
    const Result<std::string> out = render_line("x", Style{}.set_fg(Color::rgb(200, 10, 10)), opts);
    REQUIRE(out.has_value());
    CHECK(out->find("38;2;200") == std::string::npos);  // 真彩参数不得出现
    CHECK(out->find("\x1b[") != std::string::npos);
    CHECK(out->find("m") != std::string::npos);
}

TEST_CASE("api: None 档位仍产出纯文本") {
    Opts opts;
    opts.color_depth(ColorDepth::None);
    const Result<std::string> out = render_line("plain", Style{}.set_fg(Color::rgb(1, 2, 3)), opts);
    REQUIRE(out.has_value());
    CHECK(out->find("plain") != std::string::npos);
    CHECK(out->find("38;2") == std::string::npos);
}

TEST_CASE("api: init_core 幂等且可查询") {
    init_core();
    init_core();
    CHECK(core_initialized());
}

TEST_CASE("api: write_to 写成功可读回，空写与空流都是明确结果") {
    std::FILE* stream = std::tmpfile();
    REQUIRE(stream != nullptr);

    CHECK(write_to(stream, "") == ErrorCode::Ok);  // 空写不是错误
    CHECK(write_to(stream, "abc") == ErrorCode::Ok);

    std::rewind(stream);
    char buffer[8] = {};
    const std::size_t got = std::fread(buffer, 1, sizeof(buffer) - 1, stream);
    CHECK(got == 3);
    CHECK(std::string(buffer, got) == "abc");

    CHECK(write_to(nullptr, "abc") == ErrorCode::InvalidArgument);
    CHECK(flush(stream) == ErrorCode::Ok);
    CHECK(flush(nullptr) == ErrorCode::InvalidArgument);
    std::fclose(stream);
}

TEST_CASE("api: escape_for_debug 让不可打印字节可读且不回显控制符") {
    const std::string escaped = escape_for_debug(std::string("\x1b[31m\n", 6));
    CHECK(escaped.find('\x1b') == std::string::npos);
    CHECK(escaped.find('\n') == std::string::npos);
    CHECK(escaped.find("<ESC>") != std::string::npos);
    CHECK(escaped.find("<LF>") != std::string::npos);
    CHECK(escaped.find("31m") != std::string::npos);
}
