// backend/snapshot/text_backend.cpp —— 纯文本导出（含宽字符续接格跳过）
#include "termchart/backend/snapshot/text_backend.hpp"

#include "termchart/core/cell.hpp"
#include "termchart/util/utf8.hpp"

namespace termchart::backend::snapshot {

void TextBackend::render(const CellBuffer& buffer) {
    out_.clear();
    for (std::uint32_t y = 0; y < buffer.height(); ++y) {
        const std::uint32_t w = row_width(buffer, y);
        for (std::uint32_t x = 0; x < w; ++x) {
            const Cell& cell = buffer.at(x, y);
            if (is_continuation(cell)) {
                continue;
            }
            util::append(out_, cell.cp);
        }
        if (y + 1 < buffer.height()) {
            out_ += '\n';
        }
    }
}

}  // namespace termchart::backend::snapshot
