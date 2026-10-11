#include "termchart/raster/braille.hpp"

#include <cstdint>

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

    for (std::uint32_t y = 0; y < rows; ++y) {
        for (std::uint32_t x = 0; x < cols; ++x) {
            std::uint8_t dots = 0;
            for (int col = 0; col < 2; ++col) {
                for (int row = 0; row < 4; ++row) {
                    const double v = field.at(x * 2 + static_cast<std::uint32_t>(col),
                                             y * 4 + static_cast<std::uint32_t>(row));
                    bool lit = v >= opts.threshold;
                    if (opts.invert) {
                        lit = !lit;
                    }
                    if (lit) {
                        dots |= detail::braille_dots_for_cell(col, row);
                    }
                }
            }
            const char32_t cp = detail::kBrailleChars[dots];
            out.at(x, y) = Cell{cp, Style{}.set_fg(on).set_bg(off)};
        }
    }
    return out;
}

}  // namespace termchart::raster
