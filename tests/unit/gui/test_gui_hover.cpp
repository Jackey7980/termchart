// test_gui_hover.cpp —— HoverLayer 单测：命中查询、浮层叠加、未命中等价底图
#include <doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "termchart/chart/hit_region.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/gui/hover_chart.hpp"
#include "termchart/gui/scene_renderer.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/scene/scene.hpp"

using namespace termchart;

namespace {

constexpr std::uint32_t kW = 200;
constexpr std::uint32_t kH = 120;

gui::RenderOptions small_options() {
    gui::RenderOptions options;
    options.width = kW;
    options.height = kH;
    options.autofit = false;
    options.supersample = 2;
    options.text_cell_w = 6.0;
    options.text_cell_h = 10.0;
    return options;
}

// 底图：整块深灰实心矩形（保证底图本身有内容，浮层差异可归因）。
scene::Scene small_base() {
    scene::Scene scene;
    scene::SceneBuilder builder(scene);
    builder.fill(Color::rgb(Rgb{40, 40, 40}));
    builder.rect(Rect{0.0, 0.0, static_cast<double>(kW), static_cast<double>(kH)});
    return scene;
}

gui::HoverLayer make_layer() {
    std::vector<chart::HitRegion> regions;
    std::vector<std::string> labels;
    regions.push_back(chart::HitRegion::circle(Point{50.0, 40.0}, 10.0, 1));
    labels.emplace_back("Alpha\nvalue 1");
    regions.push_back(chart::HitRegion::circle(Point{150.0, 80.0}, 10.0, 2));
    labels.emplace_back("Beta\nvalue 2");
    // payload 0：登记了几何但没登记语义——命中也不该出浮层（约定见 hit_test）。
    regions.push_back(chart::HitRegion::circle(Point{120.0, 30.0}, 10.0, 0));
    return gui::HoverLayer(small_base(), std::move(regions), std::move(labels),
                           small_options());
}

std::size_t diff_pixels(const image::Bitmap& a, const image::Bitmap& b) {
    std::size_t count = 0;
    for (std::uint32_t y = 0; y < a.height(); ++y) {
        for (std::uint32_t x = 0; x < a.width(); ++x) {
            if (a.at(x, y, 0) != b.at(x, y, 0) || a.at(x, y, 1) != b.at(x, y, 1) ||
                a.at(x, y, 2) != b.at(x, y, 2)) {
                ++count;
            }
        }
    }
    return count;
}

}  // namespace

TEST_CASE("HoverLayer：命中返回对应文本，未命中/payload 0 返回空") {
    const gui::HoverLayer layer = make_layer();
    CHECK(layer.hit_label(50.0, 40.0) == "Alpha\nvalue 1");
    CHECK(layer.hit_label(150.0, 80.0) == "Beta\nvalue 2");
    CHECK(layer.hit_label(5.0, 5.0).empty());
    CHECK(layer.hit_label(120.0, 30.0).empty());  // payload 0 = 未登记语义
}

TEST_CASE("HoverLayer：悬停帧在底图上叠加浮层（尺寸不变、像素有差异）") {
    const gui::HoverLayer layer = make_layer();
    const image::Bitmap base = layer.render_base();
    CHECK(base.width() == kW);
    CHECK(base.height() == kH);

    const image::Bitmap hovered = layer.render_at(50.0, 40.0);
    CHECK(hovered.width() == base.width());
    CHECK(hovered.height() == base.height());
    CHECK(diff_pixels(base, hovered) > 0);  // 浮层确实作为图元叠进了 Scene
}

TEST_CASE("HoverLayer：未命中时渲染与底图逐像素一致") {
    const gui::HoverLayer layer = make_layer();
    const image::Bitmap base = layer.render_base();
    const image::Bitmap miss = layer.render_at(5.0, 5.0);
    CHECK(diff_pixels(base, miss) == 0);
}

TEST_CASE("HoverLayer：贴边命中时浮层翻边，仍在画布内并可见") {
    const gui::HoverLayer layer = make_layer();
    const image::Bitmap base = layer.render_base();
    // (150,80) 靠右下：默认的右上候选会越界，必须翻边/夹紧后仍画出内容。
    const image::Bitmap corner = layer.render_at(150.0, 80.0);
    CHECK(diff_pixels(base, corner) > 0);

    // 最右下角（区域外但贴边）不得越界写坏位图：尺寸与底图一致。
    const image::Bitmap edge = layer.render_at(199.0, 119.0);
    CHECK(edge.width() == base.width());
    CHECK(edge.height() == base.height());
}
