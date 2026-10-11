// backend/snapshot/html_pre_backend.cpp —— <pre> 包装导出
#include "termchart/backend/snapshot/html_pre_backend.hpp"

#include "termchart/core/cell.hpp"
#include "termchart/util/utf8.hpp"

namespace termchart::backend::snapshot {

namespace {

void append_escaped(std::string& out, char32_t cp) {
    switch (cp) {
        case U'&':
            out += "&amp;";
            return;
        case U'<':
            out += "&lt;";
            return;
        case U'>':
            out += "&gt;";
            return;
        default:
            break;
    }
    util::append(out, cp);
}

}  // namespace

void HtmlPreBackend::render(const CellBuffer& buffer) {
    out_.clear();
    out_ += "<pre>";
    for (std::uint32_t y = 0; y < buffer.height(); ++y) {
        const std::uint32_t w = row_width(buffer, y);
        for (std::uint32_t x = 0; x < w; ++x) {
            const Cell& cell = buffer.at(x, y);
            if (is_continuation(cell)) {
                continue;
            }
            append_escaped(out_, cell.cp);
        }
        if (y + 1 < buffer.height()) {
            out_ += '\n';
        }
    }
    out_ += "</pre>";
}

}  // namespace termchart::backend::snapshot
