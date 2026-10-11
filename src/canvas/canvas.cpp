#include "termchart/canvas/canvas.hpp"

#include <cmath>

#include "termchart/canvas/primitives.hpp"
#include "termchart/core/grapheme.hpp"
#include "termchart/util/grapheme.hpp"
#include "termchart/util/utf8.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::canvas {
namespace {

std::uint32_t floor_to_cell(double value) noexcept {
    if (!(value > 0.0)) {
        return 0;
    }
    return static_cast<std::uint32_t>(std::floor(value));
}

}  // namespace

Canvas::Canvas(std::uint32_t width, std::uint32_t height) : buffer_(width, height) {}

void Canvas::resize(std::uint32_t width, std::uint32_t height) { buffer_.resize(width, height); }

void Canvas::clear() { buffer_.clear(); }

void Canvas::put(Point p, char32_t cp, const Style& style) noexcept {
    put(floor_to_cell(p.x), floor_to_cell(p.y), cp, style);
}

void Canvas::put(std::uint32_t x, std::uint32_t y, char32_t cp, const Style& style) noexcept {
    if (!buffer_.contains(x, y)) {
        return;  // 越界静默丢弃：绘图路径不报错
    }
    buffer_.at(x, y) = Cell{cp, style};
}

void Canvas::fill_rect(Rect rect, char32_t cp, const Style& style) noexcept {
    const std::uint32_t x0 = floor_to_cell(rect.x);
    const std::uint32_t y0 = floor_to_cell(rect.y);
    const std::uint32_t x1 = floor_to_cell(rect.right());
    const std::uint32_t y1 = floor_to_cell(rect.bottom());
    for (std::uint32_t y = y0; y < y1 && y < buffer_.height(); ++y) {
        for (std::uint32_t x = x0; x < x1 && x < buffer_.width(); ++x) {
            buffer_.at(x, y) = Cell{cp, style};
        }
    }
}

void Canvas::draw_line(Point from, Point to, char32_t cp, const Style& style) noexcept {
    for (const Point& p : line_points(from, to)) {
        put(p, cp, style);
    }
}

void Canvas::draw_text(Point at, std::string_view text, const Style& style) noexcept {
    const std::uint32_t row = floor_to_cell(at.y);
    if (row >= buffer_.height()) {
        return;
    }
    std::uint32_t column = floor_to_cell(at.x);
    std::size_t pos = 0;

    while (pos < text.size()) {
        const std::size_t step = util::next_boundary(text, pos);
        if (step == 0) {
            break;
        }
        const util::Utf8Decode head = util::decode(text, pos);
        if (head.consumed == 0) {
            break;
        }
        const int width = util::codepoint_width(head.cp);

        // 簇内主码点之后的全部码点（组合记号）都是该格的侧面项。
        char32_t sides[termchart::kMaxSideBearings]{};
        std::size_t side_count = 0;
        std::size_t inner = pos + static_cast<std::size_t>(head.consumed);
        while (inner < pos + step && side_count < termchart::kMaxSideBearings) {
            const util::Utf8Decode d = util::decode(text, inner);
            if (d.consumed == 0) {
                break;
            }
            sides[side_count++] = d.cp;
            inner += static_cast<std::size_t>(d.consumed);
        }

        if (width == 0) {
            // 孤立零宽簇（文本以组合记号开头）：挂到前一格，绝不单独占格。
            if (column > 0 && column - 1 < buffer_.width()) {
                buffer_.set_sides(buffer_.index_of(column - 1, row), sides, side_count);
            }
        } else {
            if (column < buffer_.width()) {
                put(column, row, head.cp, style);
                if (side_count > 0) {
                    buffer_.set_sides(buffer_.index_of(column, row), sides, side_count);
                }
                // 宽字符：右侧一格是续接占位（渲染器不写它）。
                if (width == 2 && column + 1 < buffer_.width()) {
                    put(column + 1, row, kContinuationCp, style);
                }
            }
            column += static_cast<std::uint32_t>(width);
        }
        pos += step;
    }
}

const Cell& Canvas::at(std::uint32_t x, std::uint32_t y) const noexcept {
    return buffer_.at(x, y);
}

CellBuffer Canvas::to_buffer() const { return buffer_; }

}  // namespace termchart::canvas
