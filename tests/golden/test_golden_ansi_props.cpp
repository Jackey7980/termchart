// test_golden_ansi_props.cpp —— ansi_props 快照用例
//
// 覆盖三类输入：纯文本、带样式文本、东亚宽字符。
// 快照由 `python tools/gen_golden.py` 生成；**禁止手改**。
#include <doctest.h>

#include <string>

#include "golden/golden_runner.hpp"
#include "termchart/api/quick.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"

namespace {

std::string render_style(const std::string& text, const termchart::Style& style) {
    const termchart::Result<std::string> out =
        termchart::api::render_line(text, style, termchart::api::Opts{});
    REQUIRE(out.has_value());
    return *out;
}

std::string styled_bytes() {
    termchart::Style style;
    style.set_fg(termchart::Color::rgb(0x5B, 0x8F, 0xF9)).set_attr(termchart::Attr::Bold);
    return render_style("termchart", style);
}

std::string wide_bytes() { return render_style("中文宽度", termchart::Style{}); }

std::string ansi16_bytes() {
    termchart::Style style;
    style.set_fg(termchart::Color::ansi16(3)).set_bg(termchart::Color::ansi16(12));
    return render_style("AB", style);
}

std::string indexed_bytes() {
    termchart::Style style;
    style.set_fg(termchart::Color::indexed(200));
    return render_style("256", style);
}

std::string degraded_bytes() {
    termchart::Style style;
    style.set_fg(termchart::Color::rgb(0x12, 0x34, 0x56)).set_attr(termchart::Attr::Underline);
    // 降到 16 色档位：属性级快照应体现"降级后的索引色"而非原始 RGB。
    termchart::api::Opts opts;
    opts.color_depth(termchart::ColorDepth::Ansi16);
    const auto out = termchart::api::render_line("degrade", style, opts);
    REQUIRE(out.has_value());
    return *out;
}

}  // namespace

TEST_CASE("golden: 带样式文本的属性快照") {
    const golden::Props props = golden::parse_ansi(styled_bytes());
    golden::check_or_update("ansi_props", "styled_text", golden::dump(props));
}

TEST_CASE("golden: 东亚宽字符宽度") {
    const golden::Props props = golden::parse_ansi(wide_bytes());
    CHECK(props.cells.size() == 4);
    CHECK(props.width == 8);  // 四个汉字 = 8 列
    golden::check_or_update("ansi_props", "wide_chars", golden::dump(props));
}

TEST_CASE("golden: ANSI16 前景与背景") {
    const golden::Props props = golden::parse_ansi(ansi16_bytes());
    REQUIRE(props.cells.size() == 2);
    CHECK(props.cells[0].fg == "ansi16:3");
    CHECK(props.cells[0].bg == "ansi16:12");
    golden::check_or_update("ansi_props", "ansi16_pair", golden::dump(props));
}

TEST_CASE("golden: 256 索引色") {
    const golden::Props props = golden::parse_ansi(indexed_bytes());
    REQUIRE(!props.cells.empty());
    CHECK(props.cells[0].fg == "indexed:200");
    golden::check_or_update("ansi_props", "indexed_256", golden::dump(props));
}

TEST_CASE("golden: 降级到 16 色后属性快照") {
    const golden::Props props = golden::parse_ansi(degraded_bytes());
    REQUIRE(!props.cells.empty());
    CHECK(props.cells[0].fg.rfind("ansi16:", 0) == 0);
    CHECK(props.cells[0].attrs == "underline");
    golden::check_or_update("ansi_props", "degrade_to_ansi16", golden::dump(props));
}

TEST_CASE("golden: 解析器稳定性（空输入 / 仅复位）") {
    const golden::Props empty = golden::parse_ansi("");
    CHECK(empty.cells.empty());
    CHECK(empty.width == 0);

    // 复位序列不应该产生任何"格"。
    const golden::Props reset_only = golden::parse_ansi("\x1b[0m");
    CHECK(reset_only.cells.empty());
}
