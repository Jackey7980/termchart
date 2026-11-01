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

// #3 收尾：bins 分级此前只在 draw()/to_scene() 生效，rasterize()/render_bitmap()
// 走连续映射——同一份值在字符格与高清出口取到不同级色。现统一为「值→级→色」单一函数。
// 注意 vmin/vmax 按数据驱动（取所有值的最小/最大），所以只有 ≥3 个 feature 时才能构造
// 出"两个值同属一级"的情形：A=100、B=75、C=0 在 bins=2 时 A/B 都落入 bin1。
TEST_CASE("geo_map::rasterize honours bins (two values in same bin share colour)") {
    GeoMap m;
    std::string err;
    REQUIRE(m.load_geojson(R"({
      "type":"FeatureCollection","features":[
        {"type":"Feature","properties":{"name":"A"},
         "geometry":{"type":"Polygon","coordinates":[[[0,0],[20,0],[20,20],[0,20],[0,0]]]}},
        {"type":"Feature","properties":{"name":"B"},
         "geometry":{"type":"Polygon","coordinates":[[[0,-20],[20,-20],[20,0],[0,0],[0,-20]]]}},
        {"type":"Feature","properties":{"name":"C"},
         "geometry":{"type":"Polygon","coordinates":[[[0,-40],[20,-40],[20,-20],[0,-20],[0,-40]]]}}
      ]
    })", err));
    m.set_value("A", 100.0);
    m.set_value("B", 75.0);    // 与 A 同属 bin1（bins=2 时 frac 1.0 / 0.75 -> 都 bin1）
    m.set_value("C", 0.0);

    GeoMapOptions mo;
    mo.bins = 2;
    m.set_options(mo);

    GeoRasterOptions o;
    o.style = GeoRasterStyle::Braille;
    o.draw_borders = false;   // 免得边界压暗干扰比较
    const termchart::CellBuffer b = m.rasterize(20, 30, o);

    auto fg_in_third = [&](int third) -> termchart::Color {
        const int r0 = third * 10;   // 30 行 -> 三段各 10 行
        for (int y = r0; y < r0 + 10; ++y)
            for (std::uint32_t x = 0; x < b.width(); ++x)
                if (b.at(x, static_cast<std::uint32_t>(y)).cp != U' ')
                    return b.at(x, static_cast<std::uint32_t>(y)).style.fg();
        return termchart::Color{};   // 该段没上色（不应发生）
    };
    const auto top = fg_in_third(0);   // A
    const auto mid = fg_in_third(1);   // B
    const auto bot = fg_in_third(2);   // C
    CHECK(top == mid);        // A、B 同 bin -> 同色；修复前连续映射会让二者不同色
    CHECK_FALSE(top == bot);  // C 不同 bin -> 不同色
}

TEST_CASE("geo_map::rasterize bins=1 collapses every value to one colour") {
    GeoMap m;
    load_square_map(m);
    m.set_value("A", 100.0);
    m.set_value("B", 0.0);

    GeoMapOptions mo;
    mo.bins = 1;
    m.set_options(mo);

    GeoRasterOptions o;
    o.style = GeoRasterStyle::Braille;
    o.draw_borders = false;
    const termchart::CellBuffer b = m.rasterize(40, 20, o);

    const auto top = b.at(10, 3).style.fg();
    const auto bottom = b.at(10, 16).style.fg();
    CHECK(top == bottom);     // 单级 -> 所有值同色
}

// #4 收尾：分级图例（色标）。legend_bins()/render_legend() 复用 layout::draw_legend，
// render_bitmap 开启 draw_legend 时在右侧留白带画纵向色标。
TEST_CASE("geo_map::legend_bins empty when no values") {
    GeoMap m;
    load_square_map(m);
    CHECK(m.legend_bins().empty());
}

TEST_CASE("geo_map::legend_bins reflects bins and value range") {
    GeoMap m;
    load_square_map(m);
    m.set_value("A", 100.0);
    m.set_value("B", 0.0);

    GeoMapOptions mo;
    mo.bins = 4;
    m.set_options(mo);

    const auto entries = m.legend_bins();
    CHECK(entries.size() == 4);
    for (const auto& e : entries) CHECK_FALSE(e.name.empty());
    // Turbo 在 [0,1] 上单调：最低级（蓝）与最高级（红）颜色不同。
    CHECK(entries.front().color != entries.back().color);
}

TEST_CASE("geo_map::render_legend produces a labelled colorbar buffer") {
    GeoMap m;
    load_square_map(m);
    m.set_value("A", 100.0);
    m.set_value("B", 0.0);

    const auto buf = m.render_legend();
    CHECK(buf.width() > 0);
    CHECK(buf.height() >= 2);
    bool any_glyph = false;
    for (std::uint32_t y = 0; y < buf.height() && !any_glyph; ++y)
        for (std::uint32_t x = 0; x < buf.width(); ++x)
            if (buf.at(x, y).cp != U' ') { any_glyph = true; break; }
    CHECK(any_glyph);
}

TEST_CASE("geo_map::render_bitmap draws legend band when draw_legend set") {
    GeoMap m;
    load_square_map(m);
    m.set_value("A", 100.0);
    m.set_value("B", 0.0);

    GeoBitmapOptions off; off.draw_legend = false;
    const auto bmp_off = m.render_bitmap(120, 80, off);

    GeoBitmapOptions on; on.draw_legend = true; on.margin = 8.0;
    const auto bmp_on = m.render_bitmap(120, 80, on);

    // 色标带位于 [width-margin-28, width-margin) = [84,112)，中心 x≈98。
    const std::uint32_t col = 98;
    // 开启图例后该列与关闭时不同（色标渐变 vs 地图着色）。
    std::size_t diff = 0;
    for (std::uint32_t y = 0; y < 80; ++y)
        for (int c = 0; c < 3; ++c)
            if (bmp_on.at(col, y, static_cast<std::uint8_t>(c)) !=
                bmp_off.at(col, y, static_cast<std::uint8_t>(c))) ++diff;
    CHECK(diff > 0);

    // 色标带本身应是纵向渐变：顶部色 ≠ 底部色。
    const bool same = bmp_on.at(col, 10, 0) == bmp_on.at(col, 70, 0) &&
                      bmp_on.at(col, 10, 1) == bmp_on.at(col, 70, 1) &&
                      bmp_on.at(col, 10, 2) == bmp_on.at(col, 70, 2);
    CHECK_FALSE(same);
}

// #5 收尾：命中测试（HitRegion 基础）。load_square_map 中 A 在上 (lat 0..20)、
// B 在下 (lat -20..0)，经度都 0..20。点与多边形内测试应取回正确要素名。
TEST_CASE("geo_map::hit_test returns the feature under a geo coordinate") {
    GeoMap m;
    load_square_map(m);

    CHECK(m.hit_test(10.0, 10.0) == "A");    // A 内
    CHECK(m.hit_test(10.0, -10.0) == "B");   // B 内
    CHECK(m.hit_test(10.0, 30.0) == "");     // 超出所有要素
    CHECK(m.hit_test(-10.0, 10.0) == "");    // 经度越界
}
