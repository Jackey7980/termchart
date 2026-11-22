// test_interaction_spatial_index.cpp —— 空间索引与线性命中测试的严格等价性
#include <doctest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "termchart/chart/hit_region.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/interaction/hit_test.hpp"
#include "termchart/interaction/spatial_index.hpp"

namespace {

using namespace termchart;

struct Rng {
    std::uint32_t state = 12345u;
    [[nodiscard]] std::uint32_t next() noexcept {
        state = state * 1664525u + 1013904223u;
        return state;
    }
    [[nodiscard]] double unit() noexcept { return static_cast<double>(next() >> 8) / 16777216.0; }
};

std::vector<chart::HitRegion> make_regions() {
    Rng rng;
    std::vector<chart::HitRegion> regions;
    regions.reserve(602);
    for (std::size_t i = 0; i < 600; ++i) {
        const double x = rng.unit() * 100.0;
        const double y = rng.unit() * 100.0;
        const std::uint32_t id = static_cast<std::uint32_t>(i + 1);
        switch (i % 3) {
            case 0:
                regions.push_back(chart::HitRegion::rect(
                    Rect{x, y, 1.0 + rng.unit() * 8.0, 1.0 + rng.unit() * 8.0}, id));
                break;
            case 1:
                regions.push_back(
                    chart::HitRegion::circle(Point{x, y}, 0.5 + rng.unit() * 4.0, id));
                break;
            default:
                regions.push_back(chart::HitRegion::polygon(
                    {{x, y}, {x + 2.0 + rng.unit() * 6.0, y}, {x, y + 2.0 + rng.unit() * 6.0}},
                    id));
                break;
        }
    }
    // 覆盖网格外的超大区域（必进 overflow）与退化零尺寸区域。
    regions.push_back(chart::HitRegion::rect(Rect{-1000.0, -1000.0, 3000.0, 3000.0}, 9001));
    regions.push_back(chart::HitRegion::rect(Rect{50.0, 50.0, 0.0, 0.0}, 9002));
    return regions;
}

void check_equivalent(const std::vector<chart::HitRegion>& regions,
                      const interaction::HitIndex& index, double x, double y) {
    const chart::HitRegion* linear = interaction::topmost_hit(regions, x, y);
    const chart::HitRegion* indexed = index.topmost(x, y);
    if (linear == nullptr) {
        CHECK(indexed == nullptr);
    } else {
        REQUIRE(indexed != nullptr);
        CHECK(indexed->payload() == linear->payload());
        CHECK(indexed->bounds().x == linear->bounds().x);
    }
    CHECK(index.payloads(x, y) == interaction::hit_payloads(regions, x, y));
}

}  // namespace

TEST_CASE("interaction/spatial_index: 空索引与 reset") {
    interaction::HitIndex index;
    CHECK(index.empty());
    CHECK(index.size() == 0);
    CHECK(index.topmost(0.0, 0.0) == nullptr);
    CHECK(index.payloads(0.0, 0.0).empty());

    const std::vector<chart::HitRegion> regions{
        chart::HitRegion::rect(Rect{0, 0, 10, 10}, 7),
        chart::HitRegion::rect(Rect{0, 0, 5, 5}, 8),
    };
    index.reset(regions);
    CHECK_FALSE(index.empty());
    CHECK(index.size() == 2);
    REQUIRE(index.topmost(1.0, 1.0) != nullptr);
    CHECK(index.topmost(1.0, 1.0)->payload() == 8);
    CHECK(index.payloads(1.0, 1.0) == std::vector<std::uint32_t>{7, 8});

    index.reset({});
    CHECK(index.empty());
    CHECK(index.topmost(1.0, 1.0) == nullptr);
}

TEST_CASE("interaction/spatial_index: 与线性扫描严格等价（600 混合形状 + 溢出区）") {
    const std::vector<chart::HitRegion> regions = make_regions();
    const interaction::HitIndex index(regions);
    CHECK(index.size() == regions.size());

    // 覆盖内部、边界、外部；含网格外坐标，验证 overflow 恒为候选。
    const double coords[] = {-1001.0, -10.0, 0.0, 1.0, 25.0, 49.5, 50.0, 50.5,
                             75.0, 99.0, 100.0, 100.5, 110.0, 1000.0};
    for (const double x : coords) {
        for (const double y : coords) {
            check_equivalent(regions, index, x, y);
        }
    }

    Rng rng;
    for (int i = 0; i < 400; ++i) {
        check_equivalent(regions, index, rng.unit() * 120.0 - 10.0,
                         rng.unit() * 120.0 - 10.0);
    }
}

TEST_CASE("interaction/spatial_index: 非有限查询走线性语义") {
    const std::vector<chart::HitRegion> regions{
        chart::HitRegion::rect(Rect{0, 0, 10, 10}, 1),
        chart::HitRegion::rect(Rect{0, 0, 10, 10}, 2),
    };
    const interaction::HitIndex index(regions);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double x : {nan, inf, -inf}) {
        check_equivalent(regions, index, x, 1.0);
        check_equivalent(regions, index, 1.0, x);
    }
}

TEST_CASE("interaction/spatial_index: 全退化输入仍等价") {
    const std::vector<chart::HitRegion> regions{
        chart::HitRegion::rect(Rect{0, 0, 0, 0}, 1),
        chart::HitRegion::rect(Rect{1, 1, 0, 5}, 2),
        chart::HitRegion::rect(Rect{2, 2, 5, 0}, 3),
    };
    const interaction::HitIndex index(regions);
    for (double x = -1.0; x <= 4.0; x += 0.5) {
        for (double y = -1.0; y <= 4.0; y += 0.5) {
            check_equivalent(regions, index, x, y);
        }
    }
}
