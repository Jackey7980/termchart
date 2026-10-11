// test_doc_ir.cpp —— Document IR（block/span/table_model/document）
#include <doctest.h>

#include <string>

#include "termchart/document/block.hpp"
#include "termchart/document/document.hpp"
#include "termchart/document/span.hpp"
#include "termchart/document/table_model.hpp"
#include "termchart/scene/scene.hpp"

namespace {

using namespace termchart;
using namespace termchart::document;

}  // namespace

TEST_CASE("document: 各类 block 的构造与 kind") {
    Document doc;
    doc.add_block(Block::heading("Title", 2));
    doc.add_block(Block::paragraph({Span{"hello", Style{}}}));
    doc.add_block(Block::code("int x = 1;", "cpp"));

    TableModel t;
    t.set_header({"a", "b"});
    t.add_row({"1", "2"});
    t.add_row({"3", "4"});
    doc.add_block(Block::table(std::move(t)));
    doc.add_block(Block::chart(scene::Scene{}));

    REQUIRE(doc.size() == 5);
    CHECK(doc.kind_of(0) == BlockKind::Heading);
    CHECK(doc.kind_of(1) == BlockKind::Paragraph);
    CHECK(doc.kind_of(2) == BlockKind::Code);
    CHECK(doc.kind_of(3) == BlockKind::Table);
    CHECK(doc.kind_of(4) == BlockKind::Chart);

    CHECK(doc.blocks()[0].level() == 2);
    CHECK(doc.blocks()[2].language() == "cpp");
}

TEST_CASE("document: table_model 行列统计") {
    TableModel t;
    CHECK(t.empty());
    t.set_header({"x", "y"});
    t.add_row({"1"});
    CHECK(t.row_count() == 1);
    CHECK(t.column_count() == 2);  // 以最宽者为准
    CHECK_FALSE(t.empty());
}

TEST_CASE("document: to_plain_text 序列化") {
    Document doc;
    doc.add_block(Block::heading("Title", 2));
    doc.add_block(Block::paragraph({Span{"hel", Style{}}, Span{"lo", Style{}}}));
    TableModel t;
    t.set_header({"a", "b"});
    t.add_row({"1", "2"});
    doc.add_block(Block::table(std::move(t)));
    doc.add_block(Block::chart(scene::Scene{}));

    const std::string text = doc.to_plain_text();
    CHECK(text.find("## Title") != std::string::npos);
    CHECK(text.find("hello") != std::string::npos);
    CHECK(text.find("a\tb") != std::string::npos);
    CHECK(text.find("1\t2") != std::string::npos);
    CHECK(text.find("[chart]") != std::string::npos);
}
