// test_eps_backend.cpp —— EPS 后端最小测试（镜像 test_pdf_backend.cpp）
#include <doctest.h>

#include <string>

#include "termchart/backend/vector/eps_backend.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

namespace {

using namespace termchart;
using namespace termchart::backend::vector;
using namespace termchart::scene;

}  // namespace

TEST_CASE("eps::EpsBackend basic empty scene") {
    EpsBackend backend;
    Scene s;
    backend.begin();
    backend.render(s);
    backend.end();
    auto bytes = backend.str();
    REQUIRE(bytes.size() > 0);
    CHECK(bytes.compare(0, 15, "%!PS-Adobe-2.0 ") == 0);
    CHECK(bytes.find("%%BoundingBox:") != std::string::npos);
    CHECK(bytes.find("%%EOF") != std::string::npos);
}

TEST_CASE("eps::render_eps helper") {
    EpsBackend backend;
    Scene s;
    auto out = render_eps(backend, s);
    CHECK(out.compare(0, 15, "%!PS-Adobe-2.0 ") == 0);
}

TEST_CASE("eps::EpsBackend defines Helvetica + emits path ops for line") {
    EpsBackend backend;
    EpsOptions opts;
    opts.height_pt = 100.0;  // 抵消 y 翻转（Point{0,0} → "M 0 100"）
    opts.width_pt = 100.0;
    backend.set_options(opts);

    Scene s;
    s.push(StrokeOp{Color::rgb(255, 0, 0), 2.0});
    s.push(MoveTo{Point{0, 0}});
    s.push(LineTo{Point{10, 0}});
    s.push(LineTo{Point{10, 5}});
    auto out = render_eps(backend, s);

    CHECK(out.find("/F1 /Helvetica findfont") != std::string::npos);  // 字体定义
    CHECK(out.find("M 0 100") != std::string::npos);   // MoveTo（y 翻转）
    CHECK(out.find("L 10 100") != std::string::npos);  // LineTo
    CHECK(out.find("L 10 95") != std::string::npos);   // LineTo（y = 100-5）
    CHECK(out.find("S\n") != std::string::npos);       // 描边结束符
}

TEST_CASE("eps::EpsBackend emits filled rect with rgb color") {
    EpsBackend backend;
    EpsOptions opts;
    opts.height_pt = 100.0;
    opts.width_pt = 100.0;
    backend.set_options(opts);

    Scene s;
    s.push(FillOp{Color::rgb(0, 0, 255)});
    s.push(RectOp{Rect{0, 0, 4, 3}});
    auto out = render_eps(backend, s);

    CHECK(out.find("0 97 4 3 re") != std::string::npos);  // 矩形（y = 100-3）
    CHECK(out.find("f\n") != std::string::npos);          // 填充
    CHECK(out.find("0.000 0.000 1.000 rg") != std::string::npos);  // 蓝色真彩
}
