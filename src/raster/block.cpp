#include "termchart/raster/block.hpp"

#include <cmath>

#include "termchart/core/cell.hpp"
#include "termchart/detail/block_chars.hpp"

namespace termchart::raster {

CellBuffer BlockRasterizer::rasterize(const image::FloatField& field,
                                      const RasterOptions& opts) const {
    const std::uint32_t cols = field.width() / subpixel_width();
    const std::uint32_t rows = field.height() / subpixel_height();
    CellBuffer out(cols, rows);

    const Color on = opts.invert ? opts.bg : opts.fg;
    const Color off = opts.invert ? opts.fg : opts.bg;

    for (std::uint32_t y = 0; y < rows; ++y) {
        for (std::uint32_t x = 0; x < cols; ++x) {
            double v = static_cast<double>(field.at(x, y));
            if (opts.invert) {
                v = 1.0 - v;
            }
            if (v < 0.0) {
                v = 0.0;
            } else if (v > 1.0) {
                v = 1.0;
            }
            const int level = static_cast<int>(std::lround(v * 8.0));
            const char32_t cp = detail::lower_eighth(level);
            out.at(x, y) = Cell{cp, Style{}.set_fg(on).set_bg(off)};
        }
    }
    return out;
}

}  // namespace termchart::raster
