// block.cpp —— Block 工厂实现
#include "termchart/document/block.hpp"

namespace termchart::document {

Block Block::paragraph(std::vector<Span> spans) {
    Block b;
    b.kind_ = BlockKind::Paragraph;
    b.spans_ = std::move(spans);
    return b;
}

Block Block::heading(std::string text, int level) {
    Block b;
    b.kind_ = BlockKind::Heading;
    b.text_ = std::move(text);
    b.level_ = level;
    return b;
}

Block Block::code(std::string text, std::string language) {
    Block b;
    b.kind_ = BlockKind::Code;
    b.text_ = std::move(text);
    b.language_ = std::move(language);
    return b;
}

Block Block::table(TableModel model) {
    Block b;
    b.kind_ = BlockKind::Table;
    b.table_ = std::move(model);
    return b;
}

Block Block::chart(scene::Scene scene) {
    Block b;
    b.kind_ = BlockKind::Chart;
    b.scene_ = std::move(scene);
    return b;
}

}  // namespace termchart::document
