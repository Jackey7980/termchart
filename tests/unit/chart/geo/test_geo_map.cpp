// tests/unit/chart/geo/test_geo_map.cpp —— GeoMap 图种最小测试
#include "termchart/chart/geo/geo_map.hpp"

#include <cstdint>
#include <string>

#include <doctest.h>

using namespace termchart::chart;

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

// ── 高清出口 ────────────────────────────────────────────────────────────────

namespace {
void load_square_map(GeoMap& m) {
    std::string err;
    REQUIRE(m.load_geojson(R"({
      "type":"FeatureCollection","features":[
        {"type":"Feature","properties":{"name":"A"},
         "geometry":{"type":"Polygon",
                     "coordinates":[[[0,0],[20,0],[20,20],[0,20],[0,0]]]}},
        {"type":"Feature","properties":{"name":"B"},
         "geometry":{"type":"Polygon",
                     "coordinates":[[[0,-20],[20,-20],[20,0],[0,0],[0,-20]]]}}
      ]
    })", err));
}
}  // namespace

TEST_CASE("geo_map::rasterize keeps the requested cell grid") {
    GeoMap m;
    load_square_map(m);
    for (auto style : {GeoRasterStyle::Braille, GeoRasterStyle::Quadrant,
                       GeoRasterStyle::HalfBlock}) {
        GeoRasterOptions o;
        o.style = style;
        const termchart::CellBuffer b = m.rasterize(40, 12, o);
        CHECK(b.width() == 40);
        CHECK(b.height() == 12);
    }
}

TEST_CASE("geo_map::rasterize subpixel counts") {
    CHECK(geo_raster_subpixel_width(GeoRasterStyle::Braille) == 2);
    CHECK(geo_raster_subpixel_height(GeoRasterStyle::Braille) == 4);
    CHECK(geo_raster_subpixel_height(GeoRasterStyle::Quadrant) == 2);
    CHECK(geo_raster_subpixel_width(GeoRasterStyle::HalfBlock) == 1);
    CHECK(std::string(geo_raster_style_name(GeoRasterStyle::Braille)) == "braille");
}

TEST_CASE("geo_map::rasterize fills the interior (braille dots are set)") {
    GeoMap m;
    load_square_map(m);
    GeoRasterOptions o;
    o.style = GeoRasterStyle::Braille;

    auto count_lit = [](const termchart::CellBuffer& b) {
        std::size_t lit = 0;
        for (std::uint32_t y = 0; y < b.height(); ++y) {
            for (std::uint32_t x = 0; x < b.width(); ++x) {
                const auto cp = static_cast<unsigned>(b.at(x, y).cp);
                if (cp >= 0x2801 && cp <= 0x28FF) ++lit;
            }
        }
        return lit;
    };

    // 两个方块上下相接，合起来是 20 宽 × 40 高（1:2）；braille 子像素网格
    // 20×20 格 = 40×80 子像素（同样是 1:2），所以取景框被完整铺满、一格不空。
    const termchart::CellBuffer full = m.rasterize(20, 20, o);
    CHECK(count_lit(full) == full.width() * full.height());

    // 换成宽高比不匹配的 40×20（子像素 80×80）：等比取景应把图居中成
    // 中间 20 列宽的竖条，两侧留下真空白——这同时验证了"不会横向拉伸变形"。
    const termchart::CellBuffer band = m.rasterize(40, 20, o);
    CHECK(count_lit(band) == 20 * 20);
    CHECK(band.at(0, 10).cp == U' ');
    CHECK(band.at(39, 10).cp == U' ');
    CHECK(band.at(20, 10).cp != U' ');
}

TEST_CASE("geo_map::rasterize distinguishes neighbouring features by colour") {
    GeoMap m;
    load_square_map(m);
    m.set_value("A", 100.0);
    m.set_value("B", 0.0);

    GeoRasterOptions o;
    o.style = GeoRasterStyle::Braille;
    o.draw_borders = false;  // 免得边界压暗干扰比较
    const termchart::CellBuffer b = m.rasterize(20, 20, o);

    // 上半（A，高值）与下半（B，低值）应取到不同颜色。
    // 注意 Style::fg 是成员函数，少写 () 拿到的是成员函数指针——两个指针恒等，
    // 于是这个断言会"永远失败"且看不出原因。
    const auto top = b.at(10, 3).style.fg();
    const auto bottom = b.at(10, 16).style.fg();
    CHECK_FALSE((top == bottom));
}

TEST_CASE("geo_map::render_bitmap returns requested size and paints interior") {
    GeoMap m;
    load_square_map(m);
    m.set_value("A", 60.0);

    termchart::chart::GeoBitmapOptions o;
    o.margin = 4.0;
    const termchart::image::Bitmap bmp = m.render_bitmap(160, 100, o);
    CHECK(bmp.width() == 160);
    CHECK(bmp.height() == 100);

    // 中心像素必须是"陆地"色，不是背景。
    const bool is_bg = (bmp.at(80, 50, 0) == o.background.r &&
                        bmp.at(80, 50, 1) == o.background.g &&
                        bmp.at(80, 50, 2) == o.background.b);
    CHECK_FALSE(is_bg);
}

TEST_CASE("geo_map::feature_names matches loaded features") {
    GeoMap m;
    load_square_map(m);
    const auto names = m.feature_names();
    REQUIRE(names.size() == 2);
    CHECK(names[0] == "A");
    CHECK(names[1] == "B");
}

TEST_CASE("geo_map::feature_names dedups MultiPolygon pieces") {
    GeoMap m;
    std::string err;
    REQUIRE(m.load_geojson(R"({
      "type":"FeatureCollection","features":[
        {"type":"Feature","properties":{"name":"M"},
         "geometry":{"type":"MultiPolygon","coordinates":[
           [[[0,0],[1,0],[1,1],[0,0]]],
           [[[5,5],[6,5],[6,6],[5,5]]]]}},
        {"type":"Feature","properties":{"name":"N"},
         "geometry":{"type":"Polygon","coordinates":[[[2,2],[3,2],[3,3],[2,2]]]}}
      ]
    })", err));

    // 三个多边形 entry、两个 feature 名：--list 与 set_value 都按 feature 名对齐，
    // 所以这里必须是 M / N 各一次。
    CHECK(m.polygon_count() == 3);
    const auto names = m.feature_names();
    REQUIRE(names.size() == 2);
    CHECK(names[0] == "M");
    CHECK(names[1] == "N");
}

TEST_CASE("geo_map::rasterize on empty map is blank, not crash") {
    GeoMap m;
    const termchart::CellBuffer b = m.rasterize(10, 5, GeoRasterOptions{});
    CHECK(b.width() == 10);
    CHECK(b.height() == 5);
    CHECK(b.at(0, 0).cp == U' ');
}
