// document.cpp —— Document 容器实现
#include "termchart/document/document.hpp"

#include <cstddef>

namespace termchart::document {

BlockKind Document::kind_of(std::size_t index) const {
    return blocks_.at(index).kind();
}

std::string Document::to_plain_text() const {
    std::string out;
    for (const Block& b : blocks_) {
        switch (b.kind()) {
            case BlockKind::Heading: {
                const int level = b.level();
                for (int i = 0; i < level; ++i) {
                    out += '#';
                }
                out += ' ';
                out += b.text();
                out += '\n';
                break;
            }
            case BlockKind::Paragraph: {
                for (const Span& s : b.spans()) {
                    out += s.text;
                }
                out += '\n';
                break;
            }
            case BlockKind::Code: {
                out += "```";
                out += b.language();
                out += '\n';
                out += b.text();
                out += "\n```\n";
                break;
            }
            case BlockKind::Table: {
                const TableModel& t = b.table();
                for (std::size_t c = 0; c < t.header().size(); ++c) {
                    if (c != 0) {
                        out += '\t';
                    }
                    out += t.header()[c];
                }
                out += '\n';
                for (const auto& row : t.rows()) {
                    for (std::size_t c = 0; c < row.size(); ++c) {
                        if (c != 0) {
                            out += '\t';
                        }
                        out += row[c];
                    }
                    out += '\n';
                }
                break;
            }
            case BlockKind::Chart:
                out += "[chart]\n";
                break;
        }
    }
    return out;
}

}  // namespace termchart::document
