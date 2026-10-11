// test_render.cpp —— 渲染层：SGR（唯一生成点）/ diff / damage / renderer
#include <doctest.h>

#include <string>

#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/damage.hpp"
#include "termchart/render/diff.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/render/sgr.hpp"

namespace {

using namespace termchart;
using namespace termchart::render;

std::string esc(const char* tail) { return std::string("\x1b[") + tail; }

// 完整序列 = 从"默认状态"出发的全部参数：前景 + 背景都发，未设的一路给默认值
// （39 / 49）。增量路径走 append_style_delta，不发这些冗余参数。
TEST_CASE("render: SGR 是唯一生成点 —— 颜色参数格式正确") {
    Style style;
    style.set_fg(Color::rgb(1, 2, 3));
    CHECK(sgr::style_sequence(style, ColorDepth::TrueColor) == esc("38;2;1;2;3;49m"));

    Style indexed;
    indexed.set_fg(Color::indexed(200));
    CHECK(sgr::style_sequence(indexed, ColorDepth::Ansi256) == esc("38;5;200;49m"));

    Style ansi;
    ansi.set_fg(Color::ansi16(1));
    CHECK(sgr::style_sequence(ansi, ColorDepth::Ansi16) == esc("31;49m"));

    Style bright;
    bright.set_fg(Color::ansi16(9));
    CHECK(sgr::style_sequence(bright, ColorDepth::Ansi16) == esc("91;49m"));

    Style backgrounds;
    backgrounds.set_bg(Color::rgb(9, 8, 7));
    CHECK(sgr::style_sequence(backgrounds, ColorDepth::TrueColor) == esc("39;48;2;9;8;7m"));
}

TEST_CASE("render: SGR 在 None 档位不产生颜色参数") {
    Style style;
    style.set_fg(Color::rgb(1, 2, 3)).set_bg(Color::indexed(5));
    CHECK(sgr::style_sequence(style, ColorDepth::None) == esc("0m"));
}

TEST_CASE("render: 属性开关与复位") {
    Style style;
    style.set_attr(Attr::Bold).set_attr(Attr::Underline);
    CHECK(sgr::style_sequence(style, ColorDepth::None) == esc("1;4m"));
    CHECK(sgr::reset() == esc("0m"));
}

TEST_CASE("render: 增量 SGR 只输出变化部分") {
    Style from;
    Style to;
    to.set_attr(Attr::Bold);
    std::string out;
    sgr::append_style_delta(out, from, to, ColorDepth::TrueColor);
    CHECK(out == esc("1m"));

    SUBCASE("完全相同时不产生任何字节") {
        std::string same;
        sgr::append_style_delta(same, to, to, ColorDepth::TrueColor);
        CHECK(same.empty());
    }
    SUBCASE("去掉属性会发出关闭码") {
        std::string off;
        sgr::append_style_delta(off, to, from, ColorDepth::TrueColor);
        CHECK(off == esc("22m"));
    }
    SUBCASE("降级后一致则不输出") {
        Style rgb_red;
        rgb_red.set_fg(Color::rgb(255, 0, 0));
        Style ansi_red;
        ansi_red.set_fg(Color::ansi16(9));
        std::string none;
        sgr::append_style_delta(none, rgb_red, ansi_red, ColorDepth::Ansi16);
        CHECK(none.empty());  // 都降级到 ansi16:9
    }
}

TEST_CASE("render: diff 找出变化段并合并邻近段") {
    CellBuffer before(8, 1);
    CellBuffer after(8, 1);
    after.at(2, 0) = Cell{'X'};
    after.at(6, 0) = Cell{'Y'};

    const auto regions = diff(before, after, 4);
    REQUIRE_FALSE(regions.empty());
    CHECK(regions[0].row == 0);
    CHECK(regions[0].begin <= 2);
    CHECK(regions.back().end > 6);

    SUBCASE("无变化即无段") {
        CHECK(diff(before, before).empty());
    }
    SUBCASE("尺寸不同返回空，调用方走全量") {
        CellBuffer other(4, 1);
        CHECK(diff(before, other).empty());
        CHECK_FALSE(identical(before, other));
    }
}

TEST_CASE("render: DamageTracker 合并区间") {
    DamageTracker damage;
    damage.resize(10, 2);
    damage.mark(3, 0);
    damage.mark(4, 0);
    REQUIRE(damage.runs_of(0).size() == 1);
    CHECK(damage.runs_of(0)[0].begin == 3);
    CHECK(damage.runs_of(0)[0].end == 5);

    SUBCASE("不相交区间保持独立") {
        damage.mark(8, 0);
        CHECK(damage.runs_of(0).size() == 2);
        CHECK(damage.dirty(8, 0));
        CHECK_FALSE(damage.dirty(6, 0));
    }
    SUBCASE("mark_all 覆盖每行") {
        damage.mark_all();
        CHECK(damage.dirty_row(1));
        CHECK(damage.run_count() >= 2);
    }
}

TEST_CASE("render: Renderer 首帧全量、次帧增量") {
    CellBuffer buffer(6, 2);
    buffer.at(0, 0) = Cell{'a'};
    buffer.at(5, 1) = Cell{'z'};

    Renderer renderer(ColorDepth::TrueColor);
    const std::string first = renderer.render(buffer);
    CHECK_FALSE(first.empty());
    CHECK(renderer.previous() != nullptr);

    const std::string unchanged = renderer.render(buffer);
    CHECK(unchanged.empty());  // 没有变化 → 一个字节都不发

    SUBCASE("改一个单元后只输出该单元附近") {
        CellBuffer next = buffer;
        next.at(2, 0) = Cell{'b'};
        const std::string delta = renderer.render(next);
        CHECK_FALSE(delta.empty());
        CHECK(delta.size() < first.size());
        CHECK(renderer.damage().dirty(2, 0));
    }

    SUBCASE("reset 后回到全量") {
        renderer.reset();
        CHECK(renderer.previous() == nullptr);
        CHECK_FALSE(renderer.render(buffer).empty());
    }

    SUBCASE("尺寸变化自动走全量") {
        CellBuffer bigger(8, 3);
        bigger.at(7, 2) = Cell{'q'};
        CHECK_FALSE(renderer.render(bigger).empty());
    }
}

TEST_CASE("render: 输出在帧尾复位，不污染后续输出") {
    CellBuffer buffer(3, 1);
    buffer.at(0, 0) = Cell{'a', Style().set_fg(Color::rgb(1, 2, 3))};
    Renderer renderer;
    const std::string out = renderer.render(buffer);
    CHECK(out.find(sgr::reset()) != std::string::npos);
}

}  // namespace
