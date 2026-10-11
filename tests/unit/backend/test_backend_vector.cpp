// test_backend_vector.cpp —— 矢量物种（Scene → SVG）
#include <doctest.h>

#include <string>

#include "termchart/backend/vector/svg_backend.hpp"
#include "termchart/backend/vector/svg_entities.hpp"
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
    scene.push(MoveTo{Point{0, 0}});
    scene.push(LineTo{Point{10, 0}});
    scene.push(LineTo{Point{10, 5}});
    scene.push(StrokeOp{Color::rgb(255, 0, 0), 2.0});

    SvgBackend backend;
    backend.set_options(VectorOptions{.scale = 4.0, .autofit = true});
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
    scene.push(FillOp{Color::rgb(0, 0, 255)});
    scene.push(ArcOp{Point{5, 5}, 2.0, 0.0, 6.283185307179586});
    scene.push(TextOp{Point{0, 0}, "hi<>", Style{}.set_fg(Color::rgb(0, 0, 0))});

    SvgBackend backend;
    const std::string svg = render_vector(backend, scene);
    CHECK(svg.find("<circle") != std::string::npos);
    CHECK(svg.find("fill=\"#0000FF\"") != std::string::npos);
    CHECK(svg.find("hi&lt;&gt;") != std::string::npos);
}
