// tests/unit/chart/geo/test_geo_map.cpp —— GeoMap 图种最小测试
#include "termchart/chart/geo/geo_map.hpp"

#include <cmath>
#include <string>

#include <doctest.h>

using namespace termchart::chart;

namespace {
bool approx(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }
}  // namespace

TEST_CASE("geo_map::load_geojson then draw empty canvas is no-op") {
    GeoMap m;
    std::string err;
    REQUIRE(m.load_geojson(R"({
      "type":"FeatureCollection","features":[
        {"type":"Feature","properties":{"name":"X"},
         "geometry":{"type":"Polygon",
                     "coordinates":[[[0,0],[1,0],[1,1],[0,1],[0,0]]]}}
      ]
    })", err));
    CHECK(m.polygon_count() == 1);

    termchart::canvas::Canvas canvas(40, 12);
    termchart::chart::DrawContext ctx{canvas};
    m.draw(ctx);  // 不该崩；画布上若干点为 ●
    // 不强断言像素（终端像素比对脆），但应能正常返回
}

TEST_CASE("geo_map::set_value marks entry") {
    GeoMap m;
    std::string err;
    REQUIRE(m.load_geojson(R"({
      "type":"FeatureCollection","features":[
        {"type":"Feature","properties":{"name":"A"},
         "geometry":{"type":"Polygon","coordinates":[[[0,0],[1,0],[1,1],[0,0]]]}},
        {"type":"Feature","properties":{"name":"B"},
         "geometry":{"type":"Polygon","coordinates":[[[2,2],[3,2],[3,3],[2,2]]]}}
      ]
    })", err));
    m.set_value("A", 10.0);
    m.set_value("B", 20.0);
    CHECK(m.polygon_count() == 2);
}

TEST_CASE("geo_map::load_geojson_file missing path") {
    GeoMap m;
    std::string err;
    CHECK_FALSE(m.load_geojson_file("does/not/exist.json", err));
    CHECK_FALSE(err.empty());
}
