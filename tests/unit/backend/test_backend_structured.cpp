// test_backend_structured.cpp —— 结构化物种（Document → HTML / Markdown）
#include <doctest.h>

#include <string>
#include <utility>

#include "termchart/backend/structured/html_backend.hpp"
#include "termchart/backend/structured/markdown_backend.hpp"
#include "termchart/document/block.hpp"
#include "termchart/document/document.hpp"
#include "termchart/document/span.hpp"
#include "termchart/document/table_model.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/scene/scene.hpp"

namespace {

using namespace termchart;
using namespace termchart::backend::structured;
using namespace termchart::document;

Document make_doc() {
    Document doc;
    doc.add_block(Block::heading("Title", 2));
    doc.add_block(Block::paragraph({Span{"a<b", Style{}}}));
    TableModel t;
    t.set_header({"a", "b"});
    t.add_row({"1", "2"});
    doc.add_block(Block::table(std::move(t)));
    doc.add_block(Block::code("int x = 1;", "cpp"));
    doc.add_block(Block::chart(scene::Scene{}));
    return doc;
}

}  // namespace

TEST_CASE("backend/structured: HTML 结构完整且转义") {
    const Document doc = make_doc();
    HtmlBackend backend;
    const std::string html = render_structured(backend, doc);

    CHECK(html.find("<!DOCTYPE html>") != std::string::npos);
    CHECK(html.find("<h2>Title</h2>") != std::string::npos);
    CHECK(html.find("a&lt;b") != std::string::npos);
    CHECK(html.find("<table>") != std::string::npos);
    CHECK(html.find("<th>a</th>") != std::string::npos);
    CHECK(html.find("<td>1</td>") != std::string::npos);
    CHECK(html.find("<pre><code class=\"language-cpp\">") != std::string::npos);
    CHECK(html.find("[chart]") != std::string::npos);
    CHECK(html.find("</html>") != std::string::npos);
}

TEST_CASE("backend/structured: Markdown 结构") {
    const Document doc = make_doc();
    MarkdownBackend backend;
    const std::string md = render_structured(backend, doc);

    CHECK(md.find("## Title") != std::string::npos);
    CHECK(md.find("a<b") != std::string::npos);  // Markdown 不转义 <
    CHECK(md.find("| a | b |") != std::string::npos);
    CHECK(md.find("| --- | --- |") != std::string::npos);
    CHECK(md.find("| 1 | 2 |") != std::string::npos);
    CHECK(md.find("```cpp") != std::string::npos);
    CHECK(md.find("[chart]") != std::string::npos);
}

TEST_CASE("backend/structured: 非空 Chart 块内联 SVG，空 Scene 保留占位符") {
    // 空 Scene 走占位符（旧行为，examples/35 的金标靠它）。
    Document placeholder_doc;
    placeholder_doc.add_block(Block::chart(scene::Scene{}));
    HtmlBackend html_backend;
    const std::string ph_html = render_structured(html_backend, placeholder_doc);
    CHECK(ph_html.find("[chart]") != std::string::npos);

    // 有几何的 Chart 块必须真的画出图来，而不是继续输出 "[chart]"。
    scene::Scene s;
    scene::SceneBuilder sb(s);
    sb.move_to(0.0, 0.0).line_to(10.0, 5.0);
    Document doc;
    doc.add_block(Block::chart(std::move(s)));

    const std::string html = render_structured(html_backend, doc);
    CHECK(html.find("<svg") != std::string::npos);
    CHECK(html.find("<div class=\"termchart-chart\"><svg") != std::string::npos);
    CHECK(html.find("[chart]") == std::string::npos);

    MarkdownBackend md_backend;
    const std::string md = render_structured(md_backend, doc);
    CHECK(md.find("<svg") != std::string::npos);
    CHECK(md.find("[chart]") == std::string::npos);
}
