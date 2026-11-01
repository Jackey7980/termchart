// test_chart_palette.cpp —— 全局调色板打通 chart（与 widget 共用一份）
//
// 关注契约：
//   * 缺省不装色板时，default_color() 逐字保持历史 ansi16 色号——图表金标快照
//     零变化（这是"接了主题但默认行为不变"的底线）；
//   * 装上色板后，图表默认色从色板槽位派生：改一处即全局换肤，不必逐图改参数；
//   * widget 侧的 ThemePalette / kDarkTheme 与 color:: 侧是**同一个类型**（别名，
//     不是各自一份副本）。
#include <doctest.h>

#include <type_traits>

#include "termchart/chart/series.hpp"
#include "termchart/color/theme.hpp"
#include "termchart/widget/theme.hpp"

namespace {

using namespace termchart;
using namespace termchart::chart;

}  // namespace

TEST_CASE("theme: 色板下沉到 color/ 后 widget 与 chart 共用同一份") {
    // 别名而非副本：两边指的是同一个类型。
    static_assert(std::is_same_v<termchart::widget::ThemePalette, termchart::color::ThemePalette>);
    static_assert(
        std::is_same_v<decltype(termchart::widget::kDarkTheme), const termchart::color::ThemePalette>);
    // 图表侧能直接用 color:: 里的主题（chart=L12 → color=L2 合法）。
    CHECK(termchart::color::theme_by_name("light").palette == termchart::color::kLightTheme);
    CHECK(termchart::widget::theme_by_name("mono").palette == termchart::widget::kMonoTheme);
}

TEST_CASE("theme: 缺省不装色板 → default_color 保持历史 ansi16 色号") {
    CHECK(default_palette() == nullptr);
    CHECK(default_color(DefaultColor::Series) == Color::ansi16(7));
    CHECK(default_color(DefaultColor::Accent) == Color::ansi16(2));
    CHECK(default_color(DefaultColor::Warn) == Color::ansi16(1));
    CHECK(default_color(DefaultColor::Neutral) == Color::ansi16(4));
    CHECK(default_color(DefaultColor::Muted) == Color::ansi16(8));
    CHECK(default_color(DefaultColor::Cyan) == Color::ansi16(6));
    CHECK(default_color(DefaultColor::Amber) == Color::ansi16(3));
}

TEST_CASE("theme: 装上色板 → 图表默认色从色板槽位派生") {
    set_default_palette(&termchart::color::kDarkTheme);
    CHECK(default_palette() == &termchart::color::kDarkTheme);
    CHECK(default_color(DefaultColor::Series) == Color::rgb(termchart::color::kDarkTheme.series));
    CHECK(default_color(DefaultColor::Accent) == Color::rgb(termchart::color::kDarkTheme.accent));
    CHECK(default_color(DefaultColor::Warn) == Color::rgb(termchart::color::kDarkTheme.danger));
    CHECK(default_color(DefaultColor::Neutral) == Color::rgb(termchart::color::kDarkTheme.neutral));
    CHECK(default_color(DefaultColor::Muted) == Color::rgb(termchart::color::kDarkTheme.text_dim));
    CHECK(default_color(DefaultColor::Cyan) == Color::rgb(termchart::color::kDarkTheme.info));
    CHECK(default_color(DefaultColor::Amber) == Color::rgb(termchart::color::kDarkTheme.caution));

    // 卸掉后回到历史行为（不给后续用例留副作用）。
    set_default_palette(nullptr);
    CHECK(default_palette() == nullptr);
    CHECK(default_color(DefaultColor::Series) == Color::ansi16(7));
}
