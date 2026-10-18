// tests/unit/util/test_geo.cpp —— 地图相关（projection + GeoJSON 解析）的最小测试集
#include "termchart/util/geo/geojson.hpp"
#include "termchart/util/geo/projection.hpp"

#include <cmath>
#include <string>

#include <doctest.h>

using namespace termchart::util::geo;

namespace {
bool approx(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }
}  // namespace

TEST_CASE("geo::projection::equirectangular identity") {
    auto p = equirectangular(30.0, -45.0);
    CHECK(approx(p.x, -45.0));
    CHECK(approx(p.y, 30.0));
}

TEST_CASE("geo::projection::web_mercator equator") {
    auto p = web_mercator(0.0, 0.0);
    CHECK(approx(p.x, 0.0));
    CHECK(approx(p.y, 0.0));
}

TEST_CASE("geo::projection::web_mercator sign") {
    auto p_n = web_mercator(45.0, 0.0);
    auto p_s = web_mercator(-45.0, 0.0);
    CHECK(p_n.y > 0.0);
    CHECK(p_s.y < 0.0);
}

TEST_CASE("geo::projection::orthographic center") {
    // 中心点正对自己：x=y=0
    auto p = orthographic(0.0, 0.0, 0.0, 0.0);
    CHECK(approx(p.x, 0.0));
    CHECK(approx(p.y, 0.0));
}

TEST_CASE("geo::projection::lambert lat=ref → y=sin(lat)") {
    auto p = lambert_cylindrical(30.0, 0.0, 30.0);
    CHECK(approx(p.y, std::sin(30.0 * 3.14159265358979 / 180.0)));
}

TEST_CASE("geo::parse_geojson simple Polygon") {
    const std::string text = R"({
      "type": "FeatureCollection",
      "features": [
        { "type": "Feature",
          "properties": { "name": "A" },
          "geometry": { "type": "Polygon",
            "coordinates": [[[0,0],[10,0],[10,10],[0,10],[0,0]]]
          }
        }
      ]
    })";
    GeoFeatureCollection fc;
    std::string err;
    REQUIRE(parse_geojson(text, fc, err));
    REQUIRE(fc.polygons.size() == 1);
    CHECK(fc.polygons[0].rings.size() == 1);
    CHECK(fc.polygons[0].rings[0].size() == 5);
    REQUIRE(fc.polygons[0].properties.size() >= 1);
    CHECK(fc.polygons[0].properties[0].first == "name");
    CHECK(fc.polygons[0].properties[0].second == "A");
}

TEST_CASE("geo::parse_geojson MultiPolygon") {
    const std::string text = R"({
      "type": "FeatureCollection",
      "features": [
        { "type": "Feature", "properties": {"id":"X"},
          "geometry": { "type": "MultiPolygon",
            "coordinates": [[[[0,0],[1,0],[1,1],[0,1],[0,0]]],
                            [[[2,2],[3,2],[3,3],[2,3],[2,2]]]]
          }
        }
      ]
    })";
    GeoFeatureCollection fc;
    std::string err;
    REQUIRE(parse_geojson(text, fc, err));
    CHECK(fc.polygons.size() == 2);
}

TEST_CASE("geo::parse_geojson bad type") {
    GeoFeatureCollection fc; std::string err;
    CHECK_FALSE(parse_geojson("not json", fc, err));
    CHECK_FALSE(err.empty());
}

TEST_CASE("geo::parse_geojson unsupported geometry") {
    const std::string text = R"({
      "type":"FeatureCollection",
      "features":[{"type":"Feature","properties":{},
                   "geometry":{"type":"Point","coordinates":[0,0]}}]
    })";
    GeoFeatureCollection fc; std::string err;
    CHECK_FALSE(parse_geojson(text, fc, err));
}

TEST_CASE("geo::compute_bounds") {
    GeoFeatureCollection fc;
    fc.polygons.push_back({});
    fc.polygons[0].rings.push_back({{0,0},{10,5},{-5,3}});
    auto b = compute_bounds(fc);
    CHECK(approx(b.min_lat, 0.0));
    CHECK(approx(b.max_lat, 5.0));
    CHECK(approx(b.min_lon, -5.0));
    CHECK(approx(b.max_lon, 10.0));
}
