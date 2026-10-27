// test_backend_text_outline.cpp —— SVG / PDF 后端的文字轮廓化（text-to-outline）集成测试。
//
// 验证：开启且提供字体时，文字以矢量路径输出（SVG <path> / PDF 路径算子 + 填充），
// 不再依赖渲染端字体；未提供字体时回退到旧行为（SVG <text> / PDF Helvetica Tj）。
// 用合成字体（synthetic_font.hpp）保证几何确定、零外部依赖。
#include <doctest.h>

#include <string>
#include <vector>

#include "../image/synthetic_font.hpp"
#include "termchart/backend/vector/pdf_backend.hpp"
#include "termchart/backend/vector/svg_backend.hpp"
#include "termchart/core/point.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

namespace {

using namespace termchart;
using namespace termchart::backend::vector;
using termchart::testing::build_font;

scene::Scene make_text_scene() {
    scene::Scene s;
    s.push(scene::TextOp{Point{1.0, 1.0}, "A", Style{}.set_fg(Color::rgb(255, 0, 0))});
    return s;
}

}  // namespace

TEST_CASE("backend/svg: 文字轮廓化把字形转成 <path>") {
    auto font = build_font();
    scene::Scene s = make_text_scene();

    VectorOptions opts;
    opts.text_outline = true;
    opts.outline_font = font;
    SvgBackend b;
    b.set_options(opts);
    b.begin();
    b.render(s);
    b.end();
    const std::string out = b.str();

    CHECK(out.find("<path") != std::string::npos);
    CHECK(out.find("<text") == std::string::npos);  // 不再发 <text>
}

TEST_CASE("backend/svg: 未提供字体时回退 <text>") {
    scene::Scene s = make_text_scene();

    VectorOptions opts;
    opts.text_outline = true;  // 开启但无 outline_font
    SvgBackend b;
    b.set_options(opts);
    b.begin();
    b.render(s);
    b.end();
    const std::string out = b.str();

    CHECK(out.find("<text") != std::string::npos);
}

TEST_CASE("backend/pdf: 文字轮廓化把字形转成填充路径") {
    auto font = build_font();
    scene::Scene s = make_text_scene();

    PdfOptions opts;
    opts.text_outline = true;
    opts.outline_font = font;
    PdfBackend b;
    b.set_options(opts);
    b.begin();
    b.render(s);
    b.end();
    const std::string out = b.str();

    CHECK(out.find("h\n") != std::string::npos);     // 字形闭合子路径
    CHECK(out.find("f\n") != std::string::npos);     // 填充闭合路径
    CHECK(out.find("Tj") == std::string::npos);      // 不再用 Helvetica Tj
}

TEST_CASE("backend/pdf: 未提供字体时回退 Helvetica Tj") {
    scene::Scene s = make_text_scene();

    PdfOptions opts;
    opts.text_outline = true;  // 开启但无 outline_font
    PdfBackend b;
    b.set_options(opts);
    b.begin();
    b.render(s);
    b.end();
    const std::string out = b.str();

    CHECK(out.find("Tj") != std::string::npos);
}
