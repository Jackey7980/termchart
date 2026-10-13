// test_color.cpp —— 颜色层：降级 / 调色板 / 渐变 / 色表 / 命名色
#include <doctest.h>

#include <string>

#include "termchart/color/colormap.hpp"
#include "termchart/color/gradient.hpp"
#include "termchart/color/named_colors.hpp"
#include "termchart/color/palette.hpp"
#include "termchart/color/quantize.hpp"
#include "termchart/core/style.hpp"

namespace {

using namespace termchart;
using namespace termchart::color;

TEST_CASE("color: 256 索引映射可逆") {
    const Rgb original{200, 30, 90};
    const std::uint8_t index = to_index_256(original);
    CHECK(index >= 16);
    CHECK(index <= 231);  // 非灰阶必落在立方格区

    SUBCASE("灰阶走 232..255 段") {
        CHECK(to_index_256(Rgb{10, 10, 10}) >= 232);
        CHECK(to_index_256(Rgb{250, 250, 250}) == 231);
    }
    SUBCASE("标准 16 色可逆") {
        for (std::uint8_t i = 0; i < 16; ++i) {
            CHECK(from_index_16(i) == from_index_256(i));
        }
    }
}

TEST_CASE("color: degrade 按能力降档") {
    const Color truecolor = Color::rgb(10, 200, 120);
    CHECK(degrade(truecolor, ColorDepth::TrueColor).kind() == ColorKind::Rgb);
    CHECK(degrade(truecolor, ColorDepth::Ansi256).kind() == ColorKind::Indexed);
    CHECK(degrade(truecolor, ColorDepth::Ansi16).kind() == ColorKind::Ansi16);
    CHECK(degrade(truecolor, ColorDepth::None).is_none());

    SUBCASE("未指定色在任何档位都保持未指定") {
        CHECK(degrade(Color::none(), ColorDepth::TrueColor).is_none());
    }
    SUBCASE("索引色在 16 色档位被压到 ansi16") {
        CHECK(degrade(Color::indexed(200), ColorDepth::Ansi16).kind() == ColorKind::Ansi16);
    }
}

TEST_CASE("color: degrade 作用于 Style 的前背景") {
    Style style;
    style.set_fg(Color::rgb(1, 2, 3)).set_bg(Color::rgb(250, 250, 250));
    const Style degraded = degrade(style, ColorDepth::Ansi16);
    CHECK(degraded.fg().kind() == ColorKind::Ansi16);
    CHECK(degraded.bg().kind() == ColorKind::Ansi16);
}

TEST_CASE("color: 调色板取色循环不越界") {
    CHECK(palette_size(PaletteKind::Default) > 0);
    CHECK(series_color(PaletteKind::Default, 0) == palette(PaletteKind::Default)[0]);
    // 系列数超过色数时循环取模。
    const std::size_t size = palette_size(PaletteKind::Mono);
    CHECK(series_color(PaletteKind::Mono, size) == series_color(PaletteKind::Mono, 0));

    CHECK(std::string(palette_name(PaletteKind::Ocean)) == "ocean");
    CHECK(palette_by_name("不存在的名字").data() == palette(PaletteKind::Default).data());
}

TEST_CASE("color: 渐变采样与端点") {
    Gradient gradient(Rgb{0, 0, 0}, Rgb{255, 255, 255});
    CHECK(gradient.sample(0.0) == Rgb{0, 0, 0});
    CHECK(gradient.sample(1.0) == Rgb{255, 255, 255});
    CHECK(gradient.sample(-1.0) == Rgb{0, 0, 0});  // 钳位
    CHECK(gradient.sample(2.0) == Rgb{255, 255, 255});
    CHECK(gradient.stop_count() == 2);

    SUBCASE("多段渐变按位置插值") {
        Gradient three(Rgb{0, 0, 0}, Rgb{100, 0, 0});
        three.add_stop(0.5, Rgb{50, 0, 0});
        CHECK(three.sample(0.25).r < 50);
        CHECK(three.sample(0.75).r > 50);
    }
    SUBCASE("generate 产出指定数量") {
        CHECK(gradient.generate(16).size() == 16);
        CHECK(gradient.generate(0).empty());
    }
}

TEST_CASE("color: 对比色按亮度挑选") {
    CHECK(contrasting_text_color(Rgb{250, 250, 250}) == Rgb{16, 16, 16});
    CHECK(contrasting_text_color(Rgb{10, 10, 10}) == Rgb{245, 245, 245});
}

TEST_CASE("color: 色表采样在 [0,1] 上有界且端点不同") {
    for (int kind = 0; kind < kColormapCount; ++kind) {
        const auto value = static_cast<ColormapKind>(kind);
        const Rgb lo = sample(value, 0.0);
        const Rgb hi = sample(value, 1.0);
        CHECK_FALSE(lo == hi);
        CHECK(sample(value, -5.0) == lo);  // 钳位
        CHECK(sample(value, 5.0) == hi);
    }
    CHECK(std::string(colormap_name(ColormapKind::Viridis)) == "viridis");

    SUBCASE("stops 缓存尺寸正确") {
        CHECK(stops(ColormapKind::Turbo, 32).size() == 32);
        CHECK(stops(ColormapKind::Turbo, 32)[0] == sample(ColormapKind::Turbo, 0.0));
    }
}

TEST_CASE("color: Turbo 用的是官方 256 级表，级点逐字节一致") {
    // 期望值直接取自 assets/colormaps/turbo.dat（官方 Turbo 表的唯一数据源）。
    // 这条测试的作用是钉死"查表没偏级、没做近似"。
    const auto level = [](int index) {
        return sample(ColormapKind::Turbo, static_cast<double>(index) / 255.0);
    };
    CHECK(level(0) == Rgb{48, 18, 59});      // 深紫端（官方首色）
    CHECK(level(32) == Rgb{70, 107, 227});   // 蓝
    CHECK(level(64) == Rgb{40, 188, 235});   // 青
    CHECK(level(128) == Rgb{164, 252, 60});  // 黄绿（亮的峰值区）
    CHECK(level(192) == Rgb{251, 126, 33});  // 橙
    CHECK(level(255) == Rgb{122, 4, 3});     // 暗红端（官方末色）

    // stops(256) 应当就是官方表本身（stops 返回的是视图，不是拷贝）。
    const auto table = stops(ColormapKind::Turbo, 256);
    REQUIRE(table.size() == 256);
    CHECK(table[0] == Rgb{48, 18, 59});
    CHECK(table[128] == Rgb{164, 252, 60});
    CHECK(table[255] == Rgb{122, 4, 3});
}

TEST_CASE("color: 命名色查表") {
    const auto white = lookup_named_color("white");
    REQUIRE(white.has_value());
    CHECK(*white == Rgb{255, 255, 255});

    CHECK(lookup_named_color("black").has_value());
    CHECK_FALSE(lookup_named_color("not-a-color").has_value());
    CHECK_FALSE(lookup_named_color("White").has_value());  // 区分大小写

    // gray/grey 两套拼法都在（CSS 全集），且色值相同。
    const auto gray = lookup_named_color("gray");
    const auto grey = lookup_named_color("grey");
    REQUIRE(gray.has_value());
    REQUIRE(grey.has_value());
    CHECK(*gray == Rgb{128, 128, 128});
    CHECK(*grey == *gray);
}

// 表是二分查找的，所以"升序且无重复"是硬契约：排序错一位，半个表就查不到。
TEST_CASE("color: 命名色表的结构不变量") {
    CHECK(kNamedColors.size() == kNamedColorCount);
    CHECK(kNamedColorCount == 148u);  // CSS Color 命名色全集

    bool sorted = true;
    bool unique = true;
    for (std::size_t i = 1; i < kNamedColors.size(); ++i) {
        if (!(kNamedColors[i - 1].name < kNamedColors[i].name)) {
            sorted = false;
            unique = false;
            break;
        }
    }
    REQUIRE(sorted);   // 严格升序
    REQUIRE(unique);   // 无重复键

    // 表里每一个名字都必须能被二分查到（排序契约的端到端验证）。
    for (const auto& entry : kNamedColors) {
        const auto got = lookup_named_color(entry.name);
        REQUIRE(got.has_value());
        CHECK(*got == entry.value);
    }

    // 抽查与 CSS 规范一致的色值（含新补齐的那一批）。
    CHECK(*lookup_named_color("rebeccapurple") == Rgb{102, 51, 153});
    CHECK(*lookup_named_color("goldenrod") == Rgb{218, 165, 32});
    CHECK(*lookup_named_color("mediumseagreen") == Rgb{60, 179, 113});
    CHECK(*lookup_named_color("navajowhite") == Rgb{255, 222, 173});
    CHECK(*lookup_named_color("lightgoldenrodyellow") == Rgb{250, 250, 210});
    CHECK_FALSE(lookup_named_color("transparent").has_value());  // 关键字不是颜色值
}

}  // namespace
