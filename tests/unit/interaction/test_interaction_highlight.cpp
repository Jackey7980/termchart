// test_interaction_highlight.cpp —— 悬停高亮 + 锚点放大
#include <doctest.h>

#include <cmath>
#include <cstdint>
#include <limits>

#include "termchart/interaction/highlight.hpp"

namespace {

using termchart::Point;
using termchart::Rect;
using termchart::interaction::Highlight;
using termchart::interaction::HighlightOptions;

}  // namespace

TEST_CASE("interaction/highlight: 默认不激活，set_hover/clear 状态往返") {
    Highlight h;
    CHECK_FALSE(h.active());
    CHECK(h.payload() == 0);

    // payload 是不透明 id，0 也允许（由调用方自行约定含义）。
    h.set_hover(0, Point{3.5, -2.0});
    CHECK(h.active());
    CHECK(h.payload() == 0);
    CHECK(h.anchor().x == doctest::Approx(3.5));
    CHECK(h.anchor().y == doctest::Approx(-2.0));

    // 移动悬停 = 再次 set_hover，payload 与锚点一起更新。
    h.set_hover(7, Point{10.0, 20.0});
    CHECK(h.payload() == 7);
    CHECK(h.anchor().x == doctest::Approx(10.0));
    CHECK(h.anchor().y == doctest::Approx(20.0));

    h.clear();
    CHECK_FALSE(h.active());
    CHECK(h.payload() == 0);  // 未悬停时 payload 恒为 0。
}

TEST_CASE("interaction/highlight: magnify=1 视为不放大，原样返回 visible") {
    Highlight h{HighlightOptions{1.0}};
    h.set_hover(1, Point{5.0, 5.0});
    const Rect visible{0.0, 0.0, 100.0, 50.0};
    CHECK(h.magnified_around(visible) == visible);
}

TEST_CASE("interaction/highlight: 放大后锚点居中且跨度正确") {
    Highlight h{HighlightOptions{2.0}};
    h.set_hover(1, Point{25.0, 25.0});
    const Rect visible{0.0, 0.0, 100.0, 50.0};
    const Rect got = h.magnified_around(visible);

    // 跨度 = 原跨度 / magnify。
    CHECK(got.w == doctest::Approx(50.0));
    CHECK(got.h == doctest::Approx(25.0));
    // 锚点性质：锚点在放大前后都落在矩形正中心（归一化位置 0.5）。
    CHECK((got.x + got.w / 2.0) == doctest::Approx(25.0));
    CHECK((got.y + got.h / 2.0) == doctest::Approx(25.0));

    // 非 2 倍的锚点位置也成立：锚点不必在原视口中心。
    Highlight h2{HighlightOptions{4.0}};
    h2.set_hover(1, Point{80.0, 10.0});
    const Rect got2 = h2.magnified_around(visible);
    CHECK(got2.w == doctest::Approx(25.0));
    CHECK(got2.h == doctest::Approx(12.5));
    CHECK((got2.x + got2.w / 2.0) == doctest::Approx(80.0));
    CHECK((got2.y + got2.h / 2.0) == doctest::Approx(10.0));
}

TEST_CASE("interaction/highlight: magnify<=1/非有限/未悬停/空矩形 都原样返回") {
    const Rect visible{10.0, 20.0, 80.0, 40.0};

    // magnify < 1（缩小倍数没有意义）与 <= 0。
    Highlight h_small{HighlightOptions{0.5}};
    h_small.set_hover(1, Point{0.0, 0.0});
    CHECK(h_small.magnified_around(visible) == visible);

    // 非有限 magnify（NaN / +Inf）：同样视为不放大。
    Highlight h_nan{HighlightOptions{std::nan("")}};
    h_nan.set_hover(1, Point{0.0, 0.0});
    CHECK(h_nan.magnified_around(visible) == visible);
    Highlight h_inf{HighlightOptions{std::numeric_limits<double>::infinity()}};
    h_inf.set_hover(1, Point{0.0, 0.0});
    CHECK(h_inf.magnified_around(visible) == visible);

    // 退化输入不改 options_：原样保存，"视为不放大"只是判定规则。
    CHECK(std::isnan(h_nan.options().magnify));
    CHECK(h_inf.options().magnify == std::numeric_limits<double>::infinity());
    CHECK(h_small.options().magnify == doctest::Approx(0.5));

    // 未悬停（clear 之后）：没有放大中心，原样返回。
    Highlight h{HighlightOptions{2.0}};
    h.set_hover(1, Point{5.0, 5.0});
    h.clear();
    CHECK_FALSE(h.active());
    CHECK(h.magnified_around(visible) == visible);

    // 空矩形没有跨度可除，原样返回。
    Highlight h2{HighlightOptions{2.0}};
    h2.set_hover(1, Point{5.0, 5.0});
    CHECK(h2.magnified_around(Rect{}) == Rect{});
}

TEST_CASE("interaction/highlight: 非有限坐标的 set_hover 不改状态") {
    Highlight h{HighlightOptions{2.0}};
    h.set_hover(1, Point{5.0, 5.0});

    // 坏事件（NaN/Inf 坐标）不得把已有悬停清掉或篡改锚点。
    h.set_hover(2, Point{std::nan(""), 0.0});
    CHECK(h.active());
    CHECK(h.payload() == 1);
    CHECK(h.anchor().x == doctest::Approx(5.0));
    CHECK(h.anchor().y == doctest::Approx(5.0));

    h.set_hover(3, Point{0.0, std::numeric_limits<double>::infinity()});
    CHECK(h.active());
    CHECK(h.payload() == 1);
    CHECK(h.anchor().y == doctest::Approx(5.0));

    // 从未悬停时，坏事件也不得凭空激活。
    Highlight fresh;
    fresh.set_hover(9, Point{std::nan(""), std::nan("")});
    CHECK_FALSE(fresh.active());
    CHECK(fresh.payload() == 0);
}
