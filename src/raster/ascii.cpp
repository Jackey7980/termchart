#include "termchart/raster/ascii.hpp"

#include <cmath>

#include "termchart/core/cell.hpp"
#include "termchart/detail/block_chars.hpp"

namespace termchart::raster {

CellBuffer AsciiRasterizer::rasterize(const image::FloatField& field,
                                      const RasterOptions& opts) const {
    const std::uint32_t cols = field.width() / subpixel_width();
    const std::uint32_t rows = field.height() / subpixel_height();
    CellBuffer out(cols, rows);

    const Color on = opts.invert ? opts.bg : opts.fg;

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
            const char32_t cp = detail::density_char(v);
            out.at(x, y) = Cell{cp, Style{}.set_fg(on)};
        }
    }
    return out;
}

}  // namespace termchart::raster
