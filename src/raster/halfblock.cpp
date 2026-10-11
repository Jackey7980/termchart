#include "termchart/raster/halfblock.hpp"

#include <cstdint>

#include "termchart/core/cell.hpp"
#include "termchart/detail/block_chars.hpp"

namespace termchart::raster {

CellBuffer HalfBlockRasterizer::rasterize(const image::FloatField& field,
                                          const RasterOptions& opts) const {
    const std::uint32_t cols = field.width() / subpixel_width();
    const std::uint32_t rows = field.height() / subpixel_height();
    CellBuffer out(cols, rows);

    const Color on = opts.invert ? opts.bg : opts.fg;
    const Color off = opts.invert ? opts.fg : opts.bg;

    for (std::uint32_t y = 0; y < rows; ++y) {
        for (std::uint32_t x = 0; x < cols; ++x) {
            const double top = field.at(x, y * 2);
            const double bot = field.at(x, y * 2 + 1);
            bool top_lit = top >= opts.threshold;
            bool bot_lit = bot >= opts.threshold;
            if (opts.invert) {
                top_lit = !top_lit;
                bot_lit = !bot_lit;
            }

            char32_t cp = detail::kSpace;
            Color fg = off;
            Color bg = off;
            if (top_lit && bot_lit) {
                cp = detail::kFullBlock;
                fg = on;
            } else if (top_lit && !bot_lit) {
                cp = detail::kUpperHalf;  // ▀：上亮 → 前景；下为熄灭色
                fg = on;
                bg = off;
            } else if (!top_lit && bot_lit) {
                cp = detail::kLowerHalf;  // ▄：下亮 → 前景；上为熄灭色
                fg = on;
                bg = off;
            }
            out.at(x, y) = Cell{cp, Style{}.set_fg(fg).set_bg(bg)};
        }
    }
    return out;
}

}  // namespace termchart::raster
