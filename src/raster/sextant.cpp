// sextant.cpp —— 六分仪字符光栅器实现
#include "termchart/raster/sextant.hpp"

#include <cstdint>

#include "termchart/core/cell.hpp"

namespace termchart::raster {

CellBuffer SextantRasterizer::rasterize(const image::FloatField& field,
                                        const RasterOptions& opts) const {
    const std::uint32_t cols = field.width() / subpixel_width();
    const std::uint32_t rows = field.height() / subpixel_height();
    CellBuffer out(cols, rows);

    const Color on = opts.invert ? opts.bg : opts.fg;
    const Color off = opts.invert ? opts.fg : opts.bg;

    // 2×3 子网格 → 6 位掩码：col 0/1（左/右），row 0/1/2（上/中/下）。
    // 位序：左上 0 · 左中 1 · 左下 2 · 右上 3 · 右中 4 · 右下 5。
    static constexpr int kBit[2][3] = {{0, 1, 2}, {3, 4, 5}};

    for (std::uint32_t y = 0; y < rows; ++y) {
        for (std::uint32_t x = 0; x < cols; ++x) {
            int mask = 0;
            for (int col = 0; col < 2; ++col) {
                for (int row = 0; row < 3; ++row) {
                    const double v =
                        field.at(x * 2 + static_cast<std::uint32_t>(col),
                                 y * 3 + static_cast<std::uint32_t>(row));
                    bool lit = v >= opts.threshold;
                    if (opts.invert) {
                        lit = !lit;
                    }
                    if (lit) {
                        mask |= (1 << kBit[col][row]);
                    }
                }
            }
            char32_t cp;
            if (mask == 0) {
                cp = U' ';
            } else if (mask == 63) {
                cp = U'\u2588';  // 全满：实心块
            } else {
                cp = static_cast<char32_t>(0x1FB00 + mask);
            }
            out.at(x, y) = Cell{cp, Style{}.set_fg(on).set_bg(off)};
        }
    }
    return out;
}

}  // namespace termchart::raster
