// test_gui_scene_renderer.cpp —— Scene → Bitmap 渲染核心契约
//
// 断言口径：**像素**。几何的"形状正确"由超采样几何保证，这里钉住的是
// 产物性质——颜色归属（Fill/Stroke 只作用其后）、边缘抗锯齿、文本置顶、
// autofit 变换、透明背景，以及"渲染是确定性的"（同输入逐字节同输出）。
#include <doctest.h>

#include <cstddef>

#include "termchart/gui/scene_renderer.hpp"
#include "termchart/media/image/png.hpp"
#include "termchart/media/image/png_encode.hpp"
#include "termchart/scene/builder.hpp"

namespace {

using termchart::Color;
using termchart::Rgb;
using termchart::gui::RenderOptions;
using termchart::gui::SceneRenderer;
using termchart::image::Bitmap;
using termchart::scene::SceneBuilder;

RenderOptions base_opts() {
    RenderOptions options;
    options.autofit = false;
    options.supersample = 3;
    options.width = 64;
    options.height = 48;
    options.background = Rgb{20, 20, 20};
    return options;
}

// 宽容色比较：抗锯齿边缘允许 ±2 的舍入差。
bool pixel_is(const Bitmap& bitmap, std::uint32_t x, std::uint32_t y, Rgb color, int tolerance = 2) {
    const int dr = static_cast<int>(bitmap.at(x, y, 0)) - static_cast<int>(color.r);
    const int dg = static_cast<int>(bitmap.at(x, y, 1)) - static_cast<int>(color.g);
    const int db = static_cast<int>(bitmap.at(x, y, 2)) - static_cast<int>(color.b);
    auto within = [tolerance](int delta) { return delta <= tolerance && delta >= -tolerance; };
    return within(dr) && within(dg) && within(db);
}

std::size_t count_not_background(const Bitmap& bitmap, Rgb background) {
    std::size_t count = 0;
    for (std::uint32_t y = 0; y < bitmap.height(); ++y) {
        for (std::uint32_t x = 0; x < bitmap.width(); ++x) {
            if (!pixel_is(bitmap, x, y, background)) {
                ++count;
            }
        }
    }
    return count;
}

}  // namespace

TEST_CASE("gui 渲染：空场景 = 纯背景，且渲染确定性") {
    termchart::scene::Scene scene;
    SceneRenderer renderer(base_opts());
    const Bitmap first = renderer.render(scene);
    const Bitmap second = renderer.render(scene);
    REQUIRE(first.width() == 64);
    REQUIRE(first.height() == 48);
    CHECK(first == second);
    CHECK(pixel_is(first, 5, 5, Rgb{20, 20, 20}));
    CHECK(pixel_is(first, 63, 47, Rgb{20, 20, 20}));
}

TEST_CASE("gui 渲染：实心矩形精确着色，无边缘渗色") {
    using namespace termchart;
    scene::Scene scene;
    SceneBuilder builder(scene);
    builder.fill(Color::rgb(255, 0, 0));
    builder.rect(Rect{10.0, 10.0, 20.0, 10.0});

    SceneRenderer renderer(base_opts());
    const Bitmap bitmap = renderer.render(scene);

    CHECK(pixel_is(bitmap, 20, 15, Rgb{255, 0, 0}));   // 内部
    CHECK(pixel_is(bitmap, 29, 19, Rgb{255, 0, 0}));   // 右下角内
    CHECK(pixel_is(bitmap, 5, 5, Rgb{20, 20, 20}));    // 外部
    CHECK(pixel_is(bitmap, 9, 15, Rgb{20, 20, 20}));   // 左缘外一像素
    CHECK(pixel_is(bitmap, 30, 15, Rgb{20, 20, 20}));  // 右缘外一像素
}

TEST_CASE("gui 渲染：描边矩形只上边框，中心留背景") {
    using namespace termchart;
    scene::Scene scene;
    SceneBuilder builder(scene);
    builder.stroke(Color::rgb(86, 156, 214), 1.0);
    builder.rect(Rect{10.0, 10.0, 40.0, 30.0});

    SceneRenderer renderer(base_opts());
    const Bitmap bitmap = renderer.render(scene);

    CHECK(pixel_is(bitmap, 30, 25, Rgb{20, 20, 20}));  // 中心 = 背景
    const Rgb edge{86, 156, 214};
    const int mid_r = static_cast<int>(bitmap.at(10, 15, 0));
    // 边框落在像素边界上：至少显著盖过背景（中心频道过半即可，不做精确覆盖断言）。
    CHECK(mid_r > 20 + (86 - 20) / 2);
    CHECK(pixel_is(bitmap, 10, 15, edge, 80));
    CHECK(pixel_is(bitmap, 49, 25, edge, 80));  // 右缘 x=50 的外侧一像素
}

TEST_CASE("gui 渲染：折线按出现时的描边色着色") {
    using namespace termchart;
    scene::Scene scene;
    SceneBuilder builder(scene);
    builder.move_to(5.0, 24.0);
    builder.stroke(Color::rgb(229, 40, 40), 1.0);
    builder.line_to(59.0, 24.0);

    SceneRenderer renderer(base_opts());
    const Bitmap bitmap = renderer.render(scene);

    const Rgb line{229, 40, 40};
    const int mid_r = static_cast<int>(bitmap.at(30, 24, 0));
    CHECK(mid_r > 20 + (229 - 20) / 2);                // 线上显著是描边色
    CHECK(bitmap.at(30, 24, 1) < 40 + (40 - 20));      // g 通道仍以描边色为主导
    CHECK(pixel_is(bitmap, 30, 20, Rgb{20, 20, 20}));  // 线外 4 像素 = 背景
}

TEST_CASE("gui 渲染：半像素线产生抗锯齿过渡") {
    using namespace termchart;
    scene::Scene scene;
    SceneBuilder builder(scene);
    builder.move_to(5.0, 24.0);  // 线心恰压在像素 23/24 的边界上
    builder.stroke(Color::rgb(229, 229, 229), 0.5);
    builder.line_to(59.0, 24.0);

    SceneRenderer renderer(base_opts());
    const Bitmap bitmap = renderer.render(scene);

    // 两侧各拿到约 1/3 子像素覆盖：都应是"介于背景与前景之间"的过渡色。
    const int above = static_cast<int>(bitmap.at(30, 23, 0));
    const int below = static_cast<int>(bitmap.at(30, 24, 0));
    CHECK(above > 40);
    CHECK(above < 190);
    CHECK(below > 40);
    CHECK(below < 190);
    CHECK(pixel_is(bitmap, 30, 26, Rgb{20, 20, 20}));  // 远处不受影响
}

TEST_CASE("gui 渲染：文本直绘在最上层且确定性") {
    using namespace termchart;
    scene::Scene scene;
    SceneBuilder builder(scene);
    builder.text(Point{10.0, 10.0}, "Hi");
    builder.stroke(Color::rgb(200, 200, 200), 1.0);
    builder.move_to(5.0, 40.0);
    builder.line_to(59.0, 40.0);

    SceneRenderer renderer(base_opts());
    const Bitmap first = renderer.render(scene);
    const Bitmap second = renderer.render(scene);
    CHECK(first == second);
    // "Hi" 两个字形至少留下十几个前景像素（位图字体 5×7，两字符 > 10 点）。
    CHECK(count_not_background(first, Rgb{20, 20, 20}) > 30);
}

TEST_CASE("gui 渲染：ImageOp 画占位而非空白") {
    using namespace termchart;
    scene::Scene scene;
    SceneBuilder builder(scene);
    builder.image(Rect{20.0, 20.0, 12.0, 10.0}, 7);

    SceneRenderer renderer(base_opts());
    const Bitmap bitmap = renderer.render(scene);
    CHECK_FALSE(pixel_is(bitmap, 26, 25, Rgb{20, 20, 20}));  // 占位棋盘/边框
    CHECK(pixel_is(bitmap, 5, 5, Rgb{20, 20, 20}));          // 区域外仍是背景
}

TEST_CASE("gui 渲染：autofit 按 bounds 缩放铺满") {
    using namespace termchart;
    scene::Scene scene;
    SceneBuilder builder(scene);
    builder.fill(Color::rgb(0, 200, 0));
    builder.rect(Rect{0.0, 0.0, 100.0, 50.0});

    RenderOptions options = base_opts();
    options.autofit = true;
    options.margin = 0.0;
    options.width = 300;
    options.height = 150;
    SceneRenderer renderer(options);
    const Bitmap bitmap = renderer.render(scene);

    // scale = min(300/100, 150/50) = 3 → 整个画布都是这个矩形。
    CHECK(pixel_is(bitmap, 0, 0, Rgb{0, 200, 0}));
    CHECK(pixel_is(bitmap, 299, 0, Rgb{0, 200, 0}));
    CHECK(pixel_is(bitmap, 0, 149, Rgb{0, 200, 0}));
    CHECK(pixel_is(bitmap, 299, 149, Rgb{0, 200, 0}));
}

TEST_CASE("gui 渲染：透明背景只在无几何处为 0") {
    using namespace termchart;
    scene::Scene scene;
    SceneBuilder builder(scene);
    builder.fill(Color::rgb(255, 0, 0));
    builder.rect(Rect{10.0, 10.0, 20.0, 10.0});

    RenderOptions options = base_opts();
    options.transparent = true;
    SceneRenderer renderer(options);
    const Bitmap bitmap = renderer.render(scene);

    CHECK(bitmap.at(5, 5, 3) == 0);      // 背景透明
    CHECK(bitmap.at(20, 15, 3) == 255);  // 几何不透明
}

TEST_CASE("gui 渲染：PNG 编解码往返逐字节一致") {
    using namespace termchart;
    scene::Scene scene;
    SceneBuilder builder(scene);
    builder.stroke(Color::rgb(86, 156, 214), 2.0);
    builder.move_to(4.0, 30.0);
    builder.line_to(30.0, 10.0);
    builder.line_to(60.0, 38.0);
    builder.fill(Color::rgb(120, 60, 160));
    builder.rect(Rect{40.0, 30.0, 16.0, 10.0});
    builder.text(Point{6.0, 6.0}, "roundtrip");

    SceneRenderer renderer(base_opts());
    const Bitmap bitmap = renderer.render(scene);
    const std::string png = termchart::image::encode_png(bitmap);
    REQUIRE_FALSE(png.empty());

    const termchart::Result<Bitmap> decoded =
        termchart::image::decode_png(std::string_view{png});
    REQUIRE(decoded.has_value());
    CHECK(decoded.value() == bitmap);
}
