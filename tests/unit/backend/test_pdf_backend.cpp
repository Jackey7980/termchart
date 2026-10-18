// tests/unit/backend/test_pdf_backend.cpp —— PDF 后端最小测试
#include "termchart/backend/vector/pdf_backend.hpp"

#include <cstring>
#include <string>

#include <doctest.h>

using namespace termchart::backend::vector;
using termchart::scene::Scene;

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
