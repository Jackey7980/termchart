// tests/unit/backend/test_pdf_backend.cpp —— PDF 后端最小测试
#include "termchart/backend/vector/pdf_backend.hpp"

#include <cstring>
#include <string>

#include <doctest.h>

#include "termchart/core/point.hpp"
#include "termchart/scene/command.hpp"

using namespace termchart::backend::vector;
using namespace termchart;
using namespace termchart::scene;
using termchart::scene::Scene;

TEST_CASE("pdf::stroked path emits stroke color and is not filled") {
    // 回归：路径按约定只描边（与 SVG/scene_renderer 一致），必须发射 RG 描边色，
    // 且不得出现 fill+stroke 的 B 操作符（旧实现把折线用残留 fill 填成色块）。
    PdfBackend backend;
    Scene s;
    s.push(StrokeOp{Color::rgb(20, 20, 20), 1.5});
    s.push(MoveTo{Point{10.0, 10.0}});
    s.push(LineTo{Point{100.0, 80.0}});
    auto bytes = render_pdf(backend, s);
    CHECK(bytes.find("RG") != std::string::npos);   // 描边色已发射
    CHECK(bytes.find("S\n") != std::string::npos);  // 画线
    CHECK(bytes.find("\nB\n") == std::string::npos); // 路径不得被填充
}

TEST_CASE("pdf::PdfBackend basic empty scene") {
    PdfBackend backend;
    Scene s;
    backend.begin();
    backend.render(s);
    backend.end();
    auto bytes = backend.str();
    REQUIRE(bytes.size() > 0);
    CHECK(bytes.compare(0, 8, "%PDF-1.4") == 0);
    CHECK(bytes.find("%%EOF") != std::string::npos);
}

TEST_CASE("pdf::render_pdf helper") {
    PdfBackend backend;
    Scene s;
    auto out = render_pdf(backend, s);
    CHECK(out.compare(0, 8, "%PDF-1.4") == 0);
}

TEST_CASE("pdf::PdfBackend contains Font and XObject") {
    PdfBackend backend;
    Scene s;
    backend.begin();
    backend.render(s);
    backend.end();
    auto bytes = backend.str();
    CHECK(bytes.find("/Type /Font") != std::string::npos);
    CHECK(bytes.find("/Type /XObject") != std::string::npos);
    CHECK(bytes.find("xref") != std::string::npos);
}
