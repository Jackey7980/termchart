#include "termchart/raster/braille.hpp"

#include <cstdint>
#include <vector>

#include "termchart/core/cell.hpp"
#include "termchart/detail/braille_table.hpp"

namespace termchart::raster {

CellBuffer BrailleRasterizer::rasterize(const image::FloatField& field,
                                        const RasterOptions& opts) const {
    const std::uint32_t cols = field.width() / subpixel_width();
    const std::uint32_t rows = field.height() / subpixel_height();
    CellBuffer out(cols, rows);

    const Color on = opts.invert ? opts.bg : opts.fg;
    const Color off = opts.invert ? opts.fg : opts.bg;

    // 先把每格的点掩码算出来（桥接要跨格看，故分两趟）。
    std::vector<std::uint8_t> dots(static_cast<std::size_t>(cols) * rows, 0);
    for (std::uint32_t y = 0; y < rows; ++y) {
        for (std::uint32_t x = 0; x < cols; ++x) {
            std::uint8_t mask = 0;
            for (int col = 0; col < 2; ++col) {
                for (int row = 0; row < 4; ++row) {
                    const double v = field.at(x * 2 + static_cast<std::uint32_t>(col),
                                             y * 4 + static_cast<std::uint32_t>(row));
                    bool lit = v >= opts.threshold;
                    if (opts.invert) {
                        lit = !lit;
                    }
                    if (lit) {
                        mask |= detail::braille_dots_for_cell(col, row);
                    }
                }
            }
            dots[static_cast<std::size_t>(y) * cols + x] = mask;
        }
    }

    // 水平桥接：相邻两格在同一子像素行都亮了"左点"时，把两格的"右点"也点亮。
    // 起因：一条横贯的细笔画在每格里只覆盖左半格（覆盖率不足阈值）时只点亮一个点，
    // 于是长直线在屏幕上呈虚线（"一粒一粒"）。桥接把它补成连续的一行点。
    // 只在**同行相邻两格都亮左点**时触发 ⇒ 竖线与孤立点不受影响。
    if (opts.bridge_horizontal && cols > 1) {
        for (std::uint32_t y = 0; y < rows; ++y) {
            for (std::uint32_t x = 0; x + 1 < cols; ++x) {
                std::uint8_t& left = dots[static_cast<std::size_t>(y) * cols + x];
                std::uint8_t& right = dots[static_cast<std::size_t>(y) * cols + x + 1];
                for (int row = 0; row < 4; ++row) {
                    const std::uint8_t ldot = detail::braille_dots_for_cell(0, row);
                    const std::uint8_t rdot = detail::braille_dots_for_cell(1, row);
                    if ((left & ldot) != 0u && (right & ldot) != 0u) {
                        left |= rdot;
                        right |= rdot;
                    }
                }
            }
        }
    }

    for (std::uint32_t y = 0; y < rows; ++y) {
        for (std::uint32_t x = 0; x < cols; ++x) {
            const std::uint8_t mask = dots[static_cast<std::size_t>(y) * cols + x];
            out.at(x, y) = Cell{detail::kBrailleChars[mask], Style{}.set_fg(on).set_bg(off)};
        }
    }
    return out;
}

}  // namespace termchart::raster
