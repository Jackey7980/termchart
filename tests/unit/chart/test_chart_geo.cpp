// test_chart_geo.cpp —— choropleth/tile_grid/hexbin/dot_density 端到端绘制
// （结构事实断言，不做逐字节 golden；四图均为"示意地图级别"，坐标即画布格）
#include <doctest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/geo/choropleth.hpp"
#include "termchart/chart/geo/dot_density.hpp"
#include "termchart/chart/geo/hexbin.hpp"
#include "termchart/chart/geo/tile_grid.hpp"
#include "termchart/core/point.hpp"

namespace {

using namespace termchart;
using namespace termchart::chart;

std::size_t count_nonblank(const canvas::Canvas& c) {
    std::size_t n = 0;
    for (std::uint32_t y = 0; y < c.height(); ++y) {
        for (std::uint32_t x = 0; x < c.width(); ++x) {
            if (c.at(x, y).cp != U'\u0020') {
                ++n;
            }
        }
    }
    return n;
}

std::size_t count_char(const canvas::Canvas& c, char32_t cp) {
    std::size_t n = 0;
    for (std::uint32_t y = 0; y < c.height(); ++y) {
        for (std::uint32_t x = 0; x < c.width(); ++x) {
            if (c.at(x, y).cp == cp) {
                ++n;
            }
        }
    }
    return n;
}

constexpr char32_t kBlock = U'\u2588';  // █ 区域/瓦片（空名）
constexpr char32_t kHex = U'\u2B22';    // ⬢ 六边形桶
constexpr char32_t kDot = U'\u2022';    // • 密度点

std::vector<Point> row_cells(double x0, double y, std::size_t n) {
    std::vector<Point> cells;
    cells.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        cells.push_back(Point{x0 + static_cast<double>(i), y});
    }
    return cells;
}

}  // namespace

// ---------------- choropleth ----------------

TEST_CASE("chart/geo/choropleth: 区域格逐格落 █ 且格数精确") {
    canvas::Canvas canvas(40, 14);
    Choropleth chart;
    GeoRegion hot;
    hot.name = "hot";
    hot.value = 10.0;
    hot.cells = row_cells(8.0, 1.0, 4);
    GeoRegion cold;
    cold.name = "cold";
    cold.value = 2.0;
    cold.cells = row_cells(8.0, 3.0, 2);
    chart.add_region(std::move(hot));
    chart.add_region(std::move(cold));
    CHECK(chart.region_count() == 2);
    chart.draw({canvas});

    // 6 格区域 █ + 底行图例色带 3 粒 █ = 9。
    CHECK(count_char(canvas, kBlock) == 9);
    CHECK(canvas.at(8, 1).cp == kBlock);
    CHECK(canvas.at(9, 3).cp == kBlock);
}

TEST_CASE("chart/geo/choropleth: 空区域、越界与 NaN 格不崩") {
    canvas::Canvas c1(40, 14);
    Choropleth empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(40, 14);
    Choropleth chart;
    GeoRegion bad;
    bad.name = "bad";
    bad.value = 5.0;
    bad.cells = {Point{-3.0, 1.0}, Point{1000.0, 1000.0},
                 Point{std::numeric_limits<double>::quiet_NaN(), 1.0},
                 Point{8.0, 1.0}};  // 仅最后一格有效
    chart.add_region(std::move(bad));
    REQUIRE_NOTHROW(chart.draw({c2}));
    CHECK(count_char(c2, kBlock) == 1);
}

TEST_CASE("chart/geo/choropleth: NaN 值按 0 收敛、bins 收敛不崩") {
    canvas::Canvas canvas(40, 14);
    Choropleth chart;
    ChoroplethOptions opts;
    opts.bins = -3;  // 收敛到 [1, 8]
    chart.set_options(opts);
    GeoRegion r;
    r.name = "r";
    r.value = std::numeric_limits<double>::quiet_NaN();
    r.cells = row_cells(8.0, 2.0, 5);
    chart.add_region(std::move(r));
    REQUIRE_NOTHROW(chart.draw({canvas}));
    CHECK(count_char(canvas, kBlock) == 5);
}

// ---------------- tile_grid ----------------

TEST_CASE("chart/geo/tile_grid: 一区域一格、首簇字符落位") {
    canvas::Canvas canvas(40, 14);
    TileGrid chart;
    chart.add_tile(GeoTile{"A", 0, 0, 1.0});
    chart.add_tile(GeoTile{"B", 2, 0, 5.0});
    chart.add_tile(GeoTile{"", 4, 0, 9.0});  // 空名 → █
    CHECK(chart.tile_count() == 3);
    chart.draw({canvas});

    // 内容区原点 (8, 1)：瓦片位置 = 原点 + (col, row)。
    CHECK(canvas.at(8, 1).cp == U'A');
    CHECK(canvas.at(10, 1).cp == U'B');
    CHECK(canvas.at(12, 1).cp == kBlock);
}

TEST_CASE("chart/geo/tile_grid: 越界瓦片整块丢弃、空网格不绘制") {
    canvas::Canvas canvas(40, 14);
    TileGrid chart;
    chart.add_tile(GeoTile{"far", 100, 100, 1.0});
    chart.add_tile(GeoTile{"in", 1, 1, 2.0});
    chart.draw({canvas});
    CHECK(count_char(canvas, U'i') == 1);
    CHECK(count_nonblank(canvas) == 1);  // 只有界内那块

    canvas::Canvas c2(40, 14);
    TileGrid empty;
    empty.draw({c2});
    CHECK(count_nonblank(c2) == 0);
}

// ---------------- hexbin ----------------

TEST_CASE("chart/geo/hexbin: 点入桶且桶心画 ⬢") {
    canvas::Canvas canvas(40, 14);
    Hexbin chart;
    chart.add_point(2.0, 2.0);
    chart.add_point(2.0, 2.0);  // 同桶重复：计数累积
    chart.add_point(10.0, 2.0);
    chart.add_point(3.0, 6.0);
    CHECK(chart.point_count() == 4);
    chart.draw({canvas});

    // radius=2：桶心 (10,3)、(18,3)、(10,7)，各一粒 ⬢。
    CHECK(count_char(canvas, kHex) == 3);
    CHECK(canvas.at(10, 3).cp == kHex);
    CHECK(canvas.at(18, 3).cp == kHex);
    CHECK(canvas.at(10, 7).cp == kHex);
}

TEST_CASE("chart/geo/hexbin: 空点集与全 NaN 点不绘制") {
    canvas::Canvas c1(40, 14);
    Hexbin empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(40, 14);
    Hexbin nan_only;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    nan_only.add_point(nan, 1.0);
    nan_only.add_point(1.0, std::numeric_limits<double>::infinity());
    nan_only.draw({c2});
    CHECK(count_nonblank(c2) == 0);
}

// ---------------- dot_density ----------------

TEST_CASE("chart/geo/dot_density: 值/unit 个点填进区域格") {
    canvas::Canvas canvas(40, 14);
    DotDensity chart;
    DotDensityRegion r;
    r.name = "r";
    r.value = 6.0;
    r.cells = row_cells(10.0, 2.0, 6);
    chart.add_region(std::move(r));
    chart.draw({canvas});

    CHECK(count_char(canvas, kDot) == 6);
}

TEST_CASE("chart/geo/dot_density: 点数截断到格数、零值不画点") {
    canvas::Canvas canvas(40, 14);
    DotDensity chart;
    DotDensityRegion r;
    r.name = "r";
    r.value = 100.0;  // 100 点 >> 3 格 → 截断到 3
    r.cells = row_cells(10.0, 2.0, 3);
    chart.add_region(std::move(r));
    chart.draw({canvas});
    CHECK(count_char(canvas, kDot) == 3);

    canvas::Canvas c2(40, 14);
    DotDensity zero;
    DotDensityRegion z;
    z.name = "z";
    z.value = 0.0;
    z.cells = row_cells(10.0, 2.0, 3);
    zero.add_region(std::move(z));
    zero.draw({c2});
    CHECK(count_nonblank(c2) == 0);
}

TEST_CASE("chart/geo/dot_density: 空区域、非法 unit 与越界格不崩") {
    canvas::Canvas c1(40, 14);
    DotDensity empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(40, 14);
    DotDensity chart;
    DotDensityOptions opts;
    opts.unit = 0.0;  // 收敛到 1
    chart.set_options(opts);
    DotDensityRegion r;
    r.name = "r";
    r.value = 5.0;
    r.cells = {Point{-1.0, 1.0}, Point{8.0, 1.0},
               Point{std::numeric_limits<double>::quiet_NaN(), 1.0}};
    chart.add_region(std::move(r));
    REQUIRE_NOTHROW(chart.draw({c2}));
    CHECK(count_char(c2, kDot) == 1);  // 仅界内一格有效
}
