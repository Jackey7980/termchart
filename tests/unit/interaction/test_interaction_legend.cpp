// test_interaction_legend.cpp —— legend_toggle：图例点击切换
#include <doctest.h>

#include <string>
#include <vector>

#include "termchart/chart/hit_region.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/interaction/legend_toggle.hpp"

namespace {

using namespace termchart;
using namespace termchart::interaction;

TEST_CASE("interaction/legend: 登记、默认可见与标签查询") {
    LegendToggle legend;
    CHECK(legend.register_series(1, "sin"));
    CHECK(legend.register_series(2, "cos"));
    CHECK(legend.series_count() == 2);

    CHECK(legend.visible(1));
    CHECK(legend.visible(2));
    CHECK(legend.visible(99));  // 未登记视为可见（新增系列不需要先登记）
    CHECK(legend.label(1) == "sin");
    CHECK(legend.label(99).empty());

    SUBCASE("id 为 0 或重复登记被拒绝") {
        CHECK_FALSE(legend.register_series(0, "bad"));
        CHECK_FALSE(legend.register_series(1, "dup"));
        CHECK(legend.series_count() == 2);
    }
}

TEST_CASE("interaction/legend: click 翻转与状态回报") {
    LegendToggle legend;
    legend.register_series(1, "sin");
    legend.register_series(2, "cos");

    CHECK(legend.click(1));           // 翻转成功
    CHECK_FALSE(legend.visible(1));
    CHECK(legend.click(1));           // 再点回来
    CHECK(legend.visible(1));

    SUBCASE("未登记的 payload 不算状态变化") {
        CHECK_FALSE(legend.click(77));
        CHECK(legend.series_count() == 2);  // 不为它造条目
    }
    SUBCASE("payload 0 永远无效") {
        CHECK_FALSE(legend.click(0));
    }
    SUBCASE("hidden 按登记顺序输出") {
        legend.register_series(3, "tan");
        (void)legend.click(2);
        (void)legend.click(3);
        const std::vector<std::uint32_t> hidden = legend.hidden();
        REQUIRE(hidden.size() == 2);
        CHECK(hidden[0] == 2);
        CHECK(hidden[1] == 3);
    }
}

TEST_CASE("interaction/legend: click_at 走 HitRegion 命中（图例布局方决定几何）") {
    LegendToggle legend;
    legend.register_series(1, "sin");
    legend.register_series(2, "cos");

    // 图例项画在右上角：两个矩形，payload = series id。
    std::vector<chart::HitRegion> legend_regions{
        chart::HitRegion::rect(Rect{50.0, 1.0, 8.0, 1.0}, 1),
        chart::HitRegion::rect(Rect{50.0, 2.0, 8.0, 1.0}, 2),
    };

    CHECK(legend.click_at(legend_regions, 52.0, 1.0));  // 点中第一项
    CHECK_FALSE(legend.visible(1));
    CHECK(legend.visible(2));

    CHECK_FALSE(legend.click_at(legend_regions, 0.0, 0.0));  // 没点中任何项 → 无状态变化
    CHECK_FALSE(legend.visible(1));                          // 仍是隐藏

    // 命中一个 payload=0 的图元：视为未登记，不切换任何系列。
    std::vector<chart::HitRegion> stray{chart::HitRegion::rect(Rect{0.0, 0.0, 5.0, 5.0}, 0)};
    CHECK_FALSE(legend.click_at(stray, 1.0, 1.0));
}

TEST_CASE("interaction/legend: hide_all / show_all / set_visible") {
    LegendToggle legend;
    legend.register_series(1, "a");
    legend.register_series(2, "b");

    legend.hide_all();
    CHECK(legend.hidden().size() == 2);
    legend.show_all();
    CHECK(legend.hidden().empty());

    legend.set_visible(2, false);
    CHECK_FALSE(legend.visible(2));
    legend.set_visible(99, false);  // 未登记：no-op，不造条目
    CHECK(legend.series_count() == 2);
}

}  // namespace
