// 35_export_html.cpp —— 结构化导出：Document → HTML（写 stdout）
#include "termchart/api/print.hpp"
#include "termchart/backend/structured/html_backend.hpp"
#include "termchart/chart/cartesian/bar_chart.hpp"
#include "termchart/document/block.hpp"
#include "termchart/document/document.hpp"
#include "termchart/document/span.hpp"
#include "termchart/document/table_model.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    document::Document doc;
    doc.add_block(document::Block::heading("Quarterly Report", 1));
    doc.add_block(document::Block::paragraph(
        {document::Span{"revenue by region, ", Style{}},
         document::Span{"preliminary", Style{}.set_attr(Attr::Italic)}}));

    document::TableModel table;
    table.set_header({"region", "q1", "q2"});
    table.add_row({"north", "12", "15"});
    table.add_row({"south", "9", "11"});
    doc.add_block(document::Block::table(std::move(table)));
    doc.add_block(document::Block::code("SELECT region, q1, q2 FROM sales;", "sql"));
    doc.add_block(document::Block::chart(scene::Scene{}));

    backend::structured::HtmlBackend backend;
    backend.set_options(backend::structured::StructuredOptions{.title = "report"});
    const std::string html = backend::structured::render_structured(backend, doc);

    const Status st = api::write_stdout(html);
    api::flush_stdout();
    return st == ErrorCode::Ok ? 0 : 1;
}
