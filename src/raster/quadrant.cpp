#include "termchart/raster/quadrant.hpp"

#include <cstdint>

#include "termchart/core/cell.hpp"

namespace termchart::raster {

namespace {

// 点位位序：UL=1, UR=2, LL=4, LR=8。表按 pat 给出对应象限块字符。
// 标准象限块（U+2596–U+259F）覆盖 1~2 点亮 + 两种对角，3 点亮走"画熄灭象限"逻辑。
constexpr char32_t quadrant_char(std::uint8_t pat) noexcept {
    switch (pat) {
        case 0b0001: return U'\u2598';  // UL
        case 0b0010: return U'\u259D';  // UR
        case 0b0011: return U'\u259B';  // 上（UL+UR）
        case 0b0100: return U'\u2596';  // LL
        case 0b0101: return U'\u2599';  // 左（UL+LL）
        case 0b0110: return U'\u259E';  // 对角（UR+LL）
        case 0b1000: return U'\u2597';  // LR
        case 0b1001: return U'\u259A';  // 对角（UL+LR）
        case 0b1010: return U'\u259C';  // 右（UR+LR）
        case 0b1100: return U'\u259F';  // 下（LL+LR）
        default: return U'\u0020';
    }
}

constexpr int popcount4(std::uint8_t v) noexcept {
    return (v & 1) + ((v >> 1) & 1) + ((v >> 2) & 1) + ((v >> 3) & 1);
}

}  // namespace

CellBuffer QuadrantRasterizer::rasterize(const image::FloatField& field,
                                         const RasterOptions& opts) const {
    const std::uint32_t cols = field.width() / subpixel_width();
    const std::uint32_t rows = field.height() / subpixel_height();
    CellBuffer out(cols, rows);

    const Color on = opts.invert ? opts.bg : opts.fg;
    const Color off = opts.invert ? opts.fg : opts.bg;

    for (std::uint32_t y = 0; y < rows; ++y) {
        for (std::uint32_t x = 0; x < cols; ++x) {
            std::uint8_t lit = 0;
            if (static_cast<double>(field.at(x * 2, y * 2)) >= opts.threshold) lit |= 0b0001;        // UL
            if (static_cast<double>(field.at(x * 2 + 1, y * 2)) >= opts.threshold) lit |= 0b0010;    // UR
            if (static_cast<double>(field.at(x * 2, y * 2 + 1)) >= opts.threshold) lit |= 0b0100;    // LL
            if (static_cast<double>(field.at(x * 2 + 1, y * 2 + 1)) >= opts.threshold) lit |= 0b1000;  // LR

            // 反相：点亮/熄灭互换，颜色随 on/off 一起翻转。
            const std::uint8_t disp = opts.invert ? static_cast<std::uint8_t>(~lit & 0x0F) : lit;
            const int count = popcount4(disp);

            char32_t cp = U'\u0020';
            Color fg = off;
            Color bg = off;
            if (count == 0) {
                cp = U'\u0020';
            } else if (count == 4) {
                cp = U'\u2588';  // █ 全亮
                fg = on;
            } else if (count <= 2) {
                cp = quadrant_char(disp);
                fg = on;
                bg = off;
            } else {  // 3 点亮：画唯一熄灭象限为熄灭色
                const std::uint8_t unlit = static_cast<std::uint8_t>(~disp & 0x0F);
                cp = quadrant_char(unlit);
                fg = off;
                bg = on;
            }
            out.at(x, y) = Cell{cp, Style{}.set_fg(fg).set_bg(bg)};
        }
    }
    return out;
}

}  // namespace termchart::raster
