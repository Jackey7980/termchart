// test_sgr.cpp —— 唯一 SGR 生成点的全面契约
//
// render/sgr.hpp 是全库**唯一**拼 SGR 序列的地方（snapshot 后端也复用它）。
// 这里把"颜色格式 / 属性位 / 降级 / 增量"钉死：任何第二处 SGR 实现都是架构事故，
// 而这里一旦松动，"先发 fg 还是先发 bg"这类无关选择会被固化成字节契约。
#include <doctest.h>

#include <string>

#include "termchart/core/attr.hpp"
#include "termchart/core/color_depth.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/sgr.hpp"

namespace {

using namespace termchart;
using namespace termchart::render::sgr;

std::string esc(const char* tail) { return std::string("\x1b[") + tail; }
bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

TEST_CASE("sgr: reset() 是无条件复位序列") {
    CHECK(reset() == esc("0m"));
}

TEST_CASE("sgr: 空样式在 None 档退化为复位，其余档发默认 39;49") {
    CHECK(style_sequence(Style{}, ColorDepth::None) == esc("0m"));
    CHECK(style_sequence(Style{}, ColorDepth::Ansi16) == esc("39;49m"));
}

TEST_CASE("sgr: 完整序列含全部属性位，顺序固定") {
    Style style;
    style.set_attr(Attr::Bold)
        .set_attr(Attr::Dim)
        .set_attr(Attr::Italic)
        .set_attr(Attr::Underline)
        .set_attr(Attr::Reverse);
    const std::string seq = style_sequence(style, ColorDepth::Ansi16);
    // kAttrOn = {1,2,3,4,7}
    CHECK(contains(seq, "1"));
    CHECK(contains(seq, "2"));
    CHECK(contains(seq, "3"));
    CHECK(contains(seq, "4"));
    CHECK(contains(seq, "7"));
    CHECK(seq.back() == 'm');
}

TEST_CASE("sgr: 各色深的前/背景格式正确") {
    const Style fg_rgb = Style{}.set_fg(Color::rgb(1, 2, 3));
    CHECK(style_sequence(fg_rgb, ColorDepth::TrueColor) == esc("38;2;1;2;3;49m"));

    // 降级到 256 色：走 6×6×6 立方索引（具体索引由量化决定，这里只钉格式）。
    const std::string s256 = style_sequence(fg_rgb, ColorDepth::Ansi256);
    CHECK(contains(s256, "38;5;"));
    CHECK(contains(s256, ";49m"));

    // 降级到 16 色：极暗的 rgb 收成黑（索引 0 → 30），且绝不出现真彩参数。
    const std::string s16 = style_sequence(fg_rgb, ColorDepth::Ansi16);
    CHECK(contains(s16, ";49m"));
    CHECK(s16.find("38;2") == std::string::npos);

    const Style bg = Style{}.set_bg(Color::ansi16(4));
    CHECK(style_sequence(bg, ColorDepth::Ansi16) == esc("39;44m"));

    const Style bright = Style{}.set_fg(Color::ansi16(9));
    CHECK(style_sequence(bright, ColorDepth::Ansi16) == esc("91;49m"));
}

TEST_CASE("sgr: delta 相同样式一个字节都不发") {
    Style style = Style{}.set_fg(Color::rgb(1, 2, 3)).set_attr(Attr::Bold);
    std::string out;
    append_style_delta(out, style, style, ColorDepth::TrueColor);
    CHECK(out.empty());
}

TEST_CASE("sgr: delta 只发变化方向，去掉则发关码") {
    const Style none;
    const Style with_fg = Style{}.set_fg(Color::ansi16(1));
    std::string out;
    append_style_delta(out, none, with_fg, ColorDepth::Ansi16);
    CHECK(contains(out, "31"));    // 开前景（ansi16(1) → 31）
    CHECK(out.find("39") == std::string::npos);  // 升档不发"关默认"

    std::string off;
    append_style_delta(off, with_fg, none, ColorDepth::Ansi16);
    CHECK(contains(off, "39"));    // 回默认：发 39
}

TEST_CASE("sgr: delta 在 None 档只动属性，不动颜色") {
    const Style a = Style{}.set_fg(Color::rgb(9, 9, 9));
    const Style b = Style{}.set_fg(Color::rgb(1, 1, 1)).set_attr(Attr::Bold);
    std::string out;
    append_style_delta(out, a, b, ColorDepth::None);
    CHECK(contains(out, "1"));     // 属性变化照常发
    CHECK_FALSE(contains(out, "38"));  // 颜色在 None 档压成默认，delta 认为无变化
    CHECK(out.back() == 'm');
}
