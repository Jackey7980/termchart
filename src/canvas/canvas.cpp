#include "termchart/canvas/canvas.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

#include "termchart/canvas/primitives.hpp"
#include "termchart/core/grapheme.hpp"
#include "termchart/util/grapheme.hpp"
#include "termchart/util/utf8.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::canvas {
namespace {

// 浮点 → 非负格坐标：负数/NaN 归 0（供 fill_rect 的裁剪用），极大值**饱和**
// 到 uint32 上限，避免越界转换（那是 UB）。调用方一律再经 contains 兜底。
std::uint32_t floor_to_cell(double value) noexcept {
    if (!(value > 0.0)) {
        return 0;
    }
    const double floored = std::floor(value);
    constexpr double kMax = 4294967295.0;  // uint32 上限
    if (floored >= kMax) {
        return std::numeric_limits<std::uint32_t>::max();
    }
    return static_cast<std::uint32_t>(floored);
}

// 有符号下取整：NaN 返回 false（调用方丢弃）。用于文本的**真裁剪**，
// 让"负列"被跳过而不是折回第 0 列。
bool signed_floor(double value, std::int64_t& out) noexcept {
    if (!(value == value)) {  // NaN
        return false;
    }
    const double floored = std::floor(value);
    if (floored >= 9.0e18) {
        out = std::numeric_limits<std::int64_t>::max();
        return true;
    }
    if (floored <= -9.0e18) {
        out = std::numeric_limits<std::int64_t>::min();
        return true;
    }
    out = static_cast<std::int64_t>(floored);
    return true;
}

}  // namespace

Canvas::Canvas(std::uint32_t width, std::uint32_t height) : buffer_(width, height) {}

void Canvas::resize(std::uint32_t width, std::uint32_t height) { buffer_.resize(width, height); }

void Canvas::clear() { buffer_.clear(); }

void Canvas::put(Point p, char32_t cp, const Style& style) noexcept {
    // 左/上边界之外（含 NaN）直接丢弃：**不**折回第 0 列/行，
    // 否则负坐标会悄悄覆盖画布边缘。
    if (!(p.x >= 0.0) || !(p.y >= 0.0)) {
        return;
    }
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
    std::int64_t column = 0;
    if (!signed_floor(at.x, column)) {
        return;  // 起点为 NaN：整段丢弃
    }
    const std::uint32_t row = floor_to_cell(at.y);
    if (row >= buffer_.height()) {
        return;
    }
    const auto width_limit = static_cast<std::int64_t>(buffer_.width());
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
            const std::int64_t prev = column - 1;
            if (prev >= 0 && prev < width_limit) {
                buffer_.set_sides(
                    buffer_.index_of(static_cast<std::uint32_t>(prev), row), sides,
                    side_count);
            }
        } else {
            // 起点落在左边界外时**跳过**该簇（真裁剪），不折回第 0 列。
            if (column >= 0 && column < width_limit) {
                const auto col = static_cast<std::uint32_t>(column);
                put(col, row, head.cp, style);
                if (side_count > 0) {
                    buffer_.set_sides(buffer_.index_of(col, row), sides, side_count);
                }
                // 宽字符：右侧一格是续接占位（渲染器不写它）。
                if (width == 2 && column + 1 < width_limit) {
                    put(col + 1, row, kContinuationCp, style);
                }
            }
            column += static_cast<std::int64_t>(width);
        }
        pos += step;
    }
}

const Cell& Canvas::at(std::uint32_t x, std::uint32_t y) const noexcept {
    return buffer_.at(x, y);
}

CellBuffer Canvas::to_buffer() const { return buffer_; }

}  // namespace termchart::canvas
