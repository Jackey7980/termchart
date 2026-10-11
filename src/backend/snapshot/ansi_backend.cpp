// backend/snapshot/ansi_backend.cpp —— 复用 render/sgr 的整帧 ANSI 导出
#include "termchart/backend/snapshot/ansi_backend.hpp"

#include "termchart/core/cell.hpp"
#include "termchart/render/sgr.hpp"  // 收窄边：仅本文件可 include
#include "termchart/util/utf8.hpp"

namespace termchart::backend::snapshot {

void AnsiBackend::render(const CellBuffer& buffer) {
    out_.clear();
    for (std::uint32_t y = 0; y < buffer.height(); ++y) {
        const std::uint32_t w = row_width(buffer, y);
        for (std::uint32_t x = 0; x < w; ++x) {
            const Cell& cell = buffer.at(x, y);
            if (is_continuation(cell)) {
                continue;  // 宽字形右半边不产生字节
            }
            render::sgr::append_style(out_, cell.style, opts_.depth);
            util::append(out_, cell.cp);
        }
        out_ += render::sgr::reset();  // 行尾收口，避免颜色溢出到下一行
        if (y + 1 < buffer.height()) {
            out_ += '\n';
        }
    }
}

}  // namespace termchart::backend::snapshot
