// test_reference.cpp —— y 轴零线参考线 + 负区底色
#include <doctest.h>

#include <cmath>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/style.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/reference.hpp"
#include "termchart/scale/linear.hpp"
#include "termchart/scale/symlog.hpp"

namespace {
using namespace termchart;
using namespace termchart::layout;

constexpr Margin kMargin{1, 1, 3, 8};

// 与 40x12 画布 + Margin{1,1,3,8} 对应的内框矩形。
Rect inner_rect() { return Rect{8.0, 1.0, 31.0, 8.0}; }

TEST_CASE("reference: Linear 域含 0 时零线加粗且在正确行") {
    canvas::Canvas canvas(40, 12);
    const PlotArea area(40.0, 12.0, kMargin, 0.0, 10.0, -5.0, 5.0);
    draw_zero_line_y(canvas, area, area.y_scale());

    const double y0 = area.y_scale().forward(0.0);  // 居中行
    const auto row = static_cast<std::uint32_t>(std::lround(y0));
    bool bold = false;
    for (std::uint32_t x = 8; x < 39; ++x) {
        const auto& c = canvas.at(x, row);
        if (c.cp == U'\u2500' && c.style.has_attr(Attr::Bold)) {
            bold = true;
            break;
        }
    }
    CHECK(bold);
}

TEST_CASE("reference: 域不含 0 时不画零线") {
    canvas::Canvas canvas(40, 12);
    const PlotArea area(40.0, 12.0, kMargin, 0.0, 10.0, 1.0, 9.0);  // y 全正
    draw_zero_line_y(canvas, area, area.y_scale());
    bool bold_line = false;
    for (std::uint32_t y = 0; y < 12; ++y)
        for (std::uint32_t x = 0; x < 40; ++x) {
            const auto& c = canvas.at(x, y);
            if (c.cp == U'\u2500' && c.style.has_attr(Attr::Bold)) {
                bold_line = true;
            }
        }
    CHECK_FALSE(bold_line);
}

TEST_CASE("reference: 负区铺底色（域含负数）") {
    canvas::Canvas canvas(40, 12);
    const PlotArea area(40.0, 12.0, kMargin, 0.0, 10.0, -5.0, 5.0);
    const Style shade = Style{}.set_bg(Color::rgb(20, 20, 40));
    shade_negative_y(canvas, area, area.y_scale(), shade);

    const double y0 = area.y_scale().forward(0.0);
    const auto row = static_cast<std::uint32_t>(std::lround(y0)) + 1;  // 零线之下
    bool shaded = false;
    for (std::uint32_t x = 8; x < 39; ++x) {
        if (canvas.at(x, row).style.bg().kind() != ColorKind::None) {
            shaded = true;
            break;
        }
    }
    CHECK(shaded);
}

TEST_CASE("reference: 域不含负数时不铺底色") {
    canvas::Canvas canvas(40, 12);
    const PlotArea area(40.0, 12.0, kMargin, 0.0, 10.0, 1.0, 9.0);
    const Style shade = Style{}.set_bg(Color::rgb(20, 20, 40));
    shade_negative_y(canvas, area, area.y_scale(), shade);
    bool shaded = false;
    for (std::uint32_t y = 0; y < 12; ++y)
        for (std::uint32_t x = 0; x < 40; ++x)
            if (canvas.at(x, y).style.bg().kind() != ColorKind::None) {
                shaded = true;
            }
    CHECK_FALSE(shaded);
}

TEST_CASE("reference: SymLog 跨零轴同样画出加粗零线（与 Linear 同代码路径）") {
    canvas::Canvas canvas(40, 12);
    scale::SymLogScale y(-10.0, 10.0, 1.0, 9.0, 1.0);
    draw_zero_line_y(canvas, inner_rect(), y);

    const double y0 = y.forward(0.0);
    const auto row = static_cast<std::uint32_t>(std::lround(y0));
    bool bold = false;
    for (std::uint32_t x = 8; x < 39; ++x) {
        const auto& c = canvas.at(x, row);
        if (c.cp == U'\u2500' && c.style.has_attr(Attr::Bold)) {
            bold = true;
            break;
        }
    }
    CHECK(bold);
}

}  // namespace
