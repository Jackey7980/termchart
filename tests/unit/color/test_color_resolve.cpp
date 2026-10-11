// test_color_resolve.cpp —— Color / Style → RGB（GUI 与位图输出的分界点）
#include <doctest.h>

#include <cstdint>

#include "termchart/color/quantize.hpp"
#include "termchart/color/resolve.hpp"
#include "termchart/core/color_depth.hpp"
#include "termchart/core/style.hpp"

namespace {

using namespace termchart;
using namespace termchart::color;

}  // namespace

TEST_CASE("color/resolve: None 走默认色，索引色查表，真彩原样") {
    const ResolveOptions options;  // TrueColor + 默认前/背景

    CHECK(resolve_color(Color::none(), options) == options.default_fg);
    CHECK(resolve_background(Color::none(), options) == options.default_bg);

    const Color ansi1 = Color::ansi16(1);
    CHECK(resolve_color(ansi1, options) == from_index_16(1));

    const Color indexed = Color::indexed(200);
    CHECK(resolve_color(indexed, options) == from_index_256(200));

    // 真彩在真彩深度下必须原样保留（不做任何量化）
    const Rgb rgb{12, 34, 56};
    CHECK(resolve_color(Color::rgb(12, 34, 56), options) == rgb);
}

TEST_CASE("color/resolve: 深度不足时先量化，GUI 与终端观感一致") {
    ResolveOptions options;
    options.depth = ColorDepth::Ansi16;
    const Rgb rgb{200, 40, 90};

    // 16 色终端下，GUI 显示的就该是 16 色表里的那一项，而不是真彩
    const Rgb expected = from_index_16(to_index_16(rgb));
    CHECK(resolve_color(Color::rgb(200, 40, 90), options) == expected);

    options.depth = ColorDepth::Ansi256;
    CHECK(resolve_color(Color::rgb(200, 40, 90), options) ==
          from_index_256(to_index_256(rgb)));

    // 已可表示的索引色在该深度下不变
    options.depth = ColorDepth::Ansi256;
    CHECK(resolve_color(Color::indexed(42), options) == from_index_256(42));
}

TEST_CASE("color/resolve: 样式解析给出前景、背景与透明标记") {
    const ResolveOptions options;

    const Style plain;
    const ResolvedStyle resolved_plain = resolve_style(plain, options);
    CHECK(resolved_plain.fg == options.default_fg);
    CHECK(resolved_plain.bg_transparent);  // 未指定背景：GUI 应与下层混合

    const Style styled = Style{}.set_fg(Color::ansi16(2)).set_bg(Color::rgb(9, 9, 9));
    const ResolvedStyle resolved = resolve_style(styled, options);
    CHECK(resolved.fg == from_index_16(2));
    CHECK_FALSE(resolved.bg_transparent);
    CHECK(resolved.bg == Rgb{9, 9, 9});

    // 自定义默认色要生效
    ResolveOptions custom;
    custom.default_fg = Rgb{1, 2, 3};
    custom.default_bg = Rgb{4, 5, 6};
    CHECK(resolve_color(Color::none(), custom) == (Rgb{1, 2, 3}));
    CHECK(resolve_background(Color::none(), custom) == (Rgb{4, 5, 6}));
}
