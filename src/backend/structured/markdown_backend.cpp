// backend/structured/markdown_backend.cpp —— Document → Markdown 实现
#include "termchart/backend/structured/markdown_backend.hpp"

#include <string>

#include "termchart/document/block.hpp"
#include "termchart/document/span.hpp"
#include "termchart/document/table_model.hpp"

namespace termchart::backend::structured {

void MarkdownBackend::render(const document::Document& doc) {
    out_.clear();
    if (!opts_.title.empty()) {
        out_ += "# ";
        out_ += opts_.title;
        out_ += "\n\n";
    }

    for (const document::Block& b : doc.blocks()) {
        switch (b.kind()) {
            case document::BlockKind::Heading: {
                int level = b.level();
                level = level < 1 ? 1 : (level > 6 ? 6 : level);
                out_.append(static_cast<std::size_t>(level), '#');
                out_ += ' ';
                out_ += b.text();
                out_ += "\n\n";
                break;
            }
            case document::BlockKind::Paragraph: {
                for (const document::Span& s : b.spans()) {
                    out_ += s.text;
                }
                out_ += "\n\n";
                break;
            }
            case document::BlockKind::Code: {
                out_ += "```";
                out_ += b.language();
                out_ += '\n';
                out_ += b.text();
                out_ += "\n```\n\n";
                break;
            }
            case document::BlockKind::Table: {
                const document::TableModel& t = b.table();
                const std::size_t cols = t.column_count();
                if (cols == 0) {
                    break;
                }
                out_ += '|';
                for (std::size_t c = 0; c < cols; ++c) {
                    out_ += ' ';
                    if (c < t.header().size()) {
                        out_ += t.header()[c];
                    }
                    out_ += " |";
                }
                out_ += '\n';
                for (std::size_t c = 0; c < cols; ++c) {
                    out_ += "| --- ";
                }
                out_ += "|\n";
                for (const auto& row : t.rows()) {
                    out_ += '|';
                    for (std::size_t c = 0; c < cols; ++c) {
                        out_ += ' ';
                        if (c < row.size()) {
                            out_ += row[c];
                        }
                        out_ += " |";
                    }
                    out_ += '\n';
                }
                out_ += '\n';
                break;
            }
            case document::BlockKind::Chart:
                out_ += "[chart]\n\n";
                break;
        }
    }
}

}  // namespace termchart::backend::structured
