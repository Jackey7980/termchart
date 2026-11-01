// test_backend_vector.cpp —— 矢量物种（Scene → SVG）
#include <doctest.h>

#include <string>

#include "termchart/backend/vector/svg_backend.hpp"
#include "termchart/backend/vector/svg_entities.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

namespace {

using namespace termchart;
using namespace termchart::backend::vector;
using namespace termchart::scene;

}  // namespace

TEST_CASE("backend/vector: 转义与颜色映射") {
    CHECK(xml_escape("a<b>&\"'") == "a&lt;b&gt;&amp;&quot;&apos;");
    CHECK(rgb_hex(Rgb{255, 0, 0}) == "#FF0000");
    CHECK(svg_color(Color::none()) == "none");
    CHECK(svg_color(Color::rgb(0, 128, 255)) == "#0080FF");
    // ansi16 经 color/ 调色板换算成 #RRGGBB
    const std::string c = svg_color(Color::ansi16(1));
    CHECK(c.size() == 7);
    CHECK(c[0] == '#');
}

TEST_CASE("backend/vector: Scene 折线 → SVG path，autofit viewBox") {
    Scene scene;
    scene.push(StrokeOp{Color::rgb(255, 0, 0), 2.0});  // 状态在前
    scene.push(MoveTo{Point{0, 0}});
    scene.push(LineTo{Point{10, 0}});
    scene.push(LineTo{Point{10, 5}});

    SvgBackend backend;
    backend.set_options(VectorOptions{.scale = 4.0, .autofit = true, .outline_font = {}});
    const std::string svg = render_vector(backend, scene);

    CHECK(svg.rfind("<svg", 0) == 0);
    CHECK(svg.find("viewBox=\"0 0 40 20\"") != std::string::npos);  // bounds 10×5 ×4
    CHECK(svg.find("<path") != std::string::npos);
    CHECK(svg.find("M 0 0") != std::string::npos);
    CHECK(svg.find("L 10 5") != std::string::npos);
    CHECK(svg.find("stroke=\"#FF0000\"") != std::string::npos);
    CHECK(svg.find("stroke-width=\"2\"") != std::string::npos);
    CHECK(svg.find("</svg>") != std::string::npos);
}

TEST_CASE("backend/vector: 整圆用 circle，文本转义") {
    Scene scene;
    scene.push(StrokeOp{Color::rgb(0, 0, 255), 1.0});  // 状态在前
    scene.push(ArcOp{Point{5, 5}, 2.0, 0.0, 6.283185307179586});
    scene.push(TextOp{Point{0, 0}, "hi<>", Style{}.set_fg(Color::rgb(0, 0, 0))});

    SvgBackend backend;
    const std::string svg = render_vector(backend, scene);
    CHECK(svg.find("<circle") != std::string::npos);
    CHECK(svg.find("stroke=\"#0000FF\"") != std::string::npos);
    CHECK(svg.find("hi&lt;&gt;") != std::string::npos);
}

TEST_CASE("backend/vector: Fill/Stroke 只作用于其后图元（与 canvas 同构）") {
    SvgBackend backend;

    // 几何在前、Stroke 在后 → 已画折线用默认色，不被后设色改写，也不带 2.0 宽度。
    Scene late;
    late.push(MoveTo{Point{0, 0}});
    late.push(LineTo{Point{10, 0}});
    late.push(StrokeOp{Color::rgb(255, 0, 0), 2.0});
    const std::string s_late = render_vector(backend, late);
    CHECK(s_late.find("stroke=\"currentColor\"") != std::string::npos);
    CHECK(s_late.find("stroke=\"#FF0000\"") == std::string::npos);
    CHECK(s_late.find("stroke-width=\"2\"") == std::string::npos);

    // Stroke 在前、几何在后 → 折线用该色。
    Scene early;
    early.push(StrokeOp{Color::rgb(255, 0, 0), 2.0});
    early.push(MoveTo{Point{0, 0}});
    early.push(LineTo{Point{10, 0}});
    const std::string s_early = render_vector(backend, early);
    CHECK(s_early.find("stroke=\"#FF0000\"") != std::string::npos);

    // Fill 在 Rect 前 → 填充矩形；Fill 在后 → 只剩描边矩形。
    Scene filled;
    filled.push(FillOp{Color::rgb(0, 0, 255)});
    filled.push(RectOp{Rect{0, 0, 4, 3}});
    const std::string s_fill = render_vector(backend, filled);
    CHECK(s_fill.find("fill=\"#0000FF\"") != std::string::npos);

    Scene outlined;
    outlined.push(RectOp{Rect{0, 0, 4, 3}});
    outlined.push(FillOp{Color::rgb(0, 0, 255)});
    const std::string s_outline = render_vector(backend, outlined);
    CHECK(s_outline.find("fill=\"none\"") != std::string::npos);
    CHECK(s_outline.find("fill=\"#0000FF\"") == std::string::npos);
}
