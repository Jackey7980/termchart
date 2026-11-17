// test_full_render.cpp —— 端到端渲染链路
//
// 链路：文本 + Style → CellBuffer → Renderer（diff/damage）→ 字节流 → 解析回属性。
// 断言的重点不是字节相等（那由 ansi_props golden 负责），
// 而是"链路各环节的契约"：增量、尺寸变化、降级、组合记号挂载。
#include <doctest.h>

#include <string>

#include "golden/golden_runner.hpp"
#include "termchart/api/opts.hpp"
#include "termchart/api/quick.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

TEST_CASE("integration: quick::init_core 幂等") {
    api::init_core();
    api::init_core();
    CHECK(api::core_initialized());
}

TEST_CASE("integration: 一行文本走完整链路可解析回属性") {
    Style style;
    style.set_fg(Color::rgb(91, 143, 249)).set_attr(Attr::Bold);

    const auto rendered = api::render_line("hello", style);
    REQUIRE(rendered.has_value());

    const golden::Props props = golden::parse_ansi(*rendered);
    REQUIRE(props.cells.size() == 5);
    CHECK(props.width == 5);
    CHECK(props.height == 1);
    CHECK(props.cells[0].codepoint == U'h');
    CHECK(props.cells[0].attrs == "bold");
    CHECK(props.cells[0].fg == "#5b8ff9");
}

TEST_CASE("integration: 指定宽度时用空格补齐") {
    api::ApiOptions opts;
    opts.width(10).newline(false);
    const auto rendered = api::render_line("ab", Style{}, opts);
    REQUIRE(rendered.has_value());
    const golden::Props props = golden::parse_ansi(*rendered);
    CHECK(props.width == 10);
}

TEST_CASE("integration: 组合记号挂到侧面表而非另占一格") {
    const auto rendered = api::render_line("e\u0301", Style{});
    REQUIRE(rendered.has_value());
    const golden::Props props = golden::parse_ansi(*rendered);
    CHECK(props.width == 1);  // 显示宽度仍是 1
    REQUIRE(props.cells.size() == 1);
    CHECK(props.cells[0].codepoint == U'e');
}

TEST_CASE("integration: 非法宽度被拒绝而非崩溃") {
    api::ApiOptions opts;
    opts.width(0);
    const auto rendered = api::render_line("x", Style{}, opts);
    CHECK_FALSE(rendered.has_value());
    CHECK(rendered.error() == ErrorCode::InvalidArgument);
}

TEST_CASE("integration: 渲染器增量 + damage 覆盖改动单元") {
    render::Renderer renderer;
    CellBuffer buffer(4, 1);
    buffer.at(0, 0) = Cell{'a'};
    CHECK_FALSE(renderer.render(buffer).empty());
    CHECK(renderer.render(buffer).empty());  // 第二次无变化

    CellBuffer changed = buffer;
    changed.at(2, 0) = Cell{'b'};
    CHECK_FALSE(renderer.render(changed).empty());
    CHECK(renderer.damage().dirty(2, 0));
    CHECK_FALSE(renderer.damage().dirty(0, 0));
}

TEST_CASE("integration: 全量渲染在深度变化后不会留下旧状态") {
    render::Renderer renderer(ColorDepth::TrueColor);
    CellBuffer buffer(3, 1);
    buffer.at(0, 0) = Cell{'x', Style().set_fg(Color::rgb(1, 2, 3))};
    const std::string truecolor = renderer.render_full(buffer);
    CHECK(truecolor.find("38;2") != std::string::npos);

    renderer.set_depth(ColorDepth::Ansi16);
    const std::string ansi = renderer.render_full(buffer);
    CHECK(ansi.find("38;2") == std::string::npos);
    // doctest 的表达式分解不接受 `a || b`，加一层括号视为单个布尔表达式。
    CHECK((ansi.find("31m") != std::string::npos || ansi.find("30m") != std::string::npos));
}

}  // namespace
