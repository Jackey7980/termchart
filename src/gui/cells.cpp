#include "termchart/gui/cells.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include "termchart/color/resolve.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/detail/braille_table.hpp"
#include "termchart/gui/font.hpp"

namespace termchart::gui {
namespace {

// 矩形填充（带 alpha：透明背景时写 0，供再合成）。
void fill_rect(image::Bitmap& bmp, double x, double y, double w, double h, const Rgb& rgb,
               std::uint8_t alpha) {
    const double fw = static_cast<double>(bmp.width());
    const double fh = static_cast<double>(bmp.height());
    const std::uint32_t x0 = static_cast<std::uint32_t>(std::max(0.0, std::floor(x)));
    const std::uint32_t y0 = static_cast<std::uint32_t>(std::max(0.0, std::floor(y)));
    const std::uint32_t x1 =
        static_cast<std::uint32_t>(std::min(fw, std::ceil(x + std::max(0.0, w))));
    const std::uint32_t y1 =
        static_cast<std::uint32_t>(std::min(fh, std::ceil(y + std::max(0.0, h))));
    for (std::uint32_t py = y0; py < y1; ++py) {
        for (std::uint32_t px = x0; px < x1; ++px) {
            bmp.set(px, py, 0, rgb.r);
            bmp.set(px, py, 1, rgb.g);
            bmp.set(px, py, 2, rgb.b);
            bmp.set(px, py, 3, alpha);
        }
    }
}

// 单码点 → UTF-8。
std::string utf8_of(char32_t cp) {
    std::string out;
    if (cp < 0x80U) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800U) {
        out.push_back(static_cast<char>(0xC0U | (cp >> 6U)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    } else {
        out.push_back(static_cast<char>(0xE0U | (cp >> 12U)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    }
    return out;
}

// 盲文：2×4 点阵。点距取满格距 ⇒ 横向笔画连成实笔画（与终端观感一致）。
void draw_braille(image::Bitmap& bmp, char32_t cp, double x, double y, double cw, double ch,
                  const Rgb& fg) {
    const std::uint8_t bits = static_cast<std::uint8_t>(cp - 0x2800U);
    const double dw = std::max(1.0, std::floor(cw / 2.0));
    const double dh = std::max(1.0, std::floor(ch / 4.0));
    const double ox = x + (cw - 2.0 * dw) / 2.0;
    const double oy = y + (ch - 4.0 * dh) / 2.0;
    for (int col = 0; col < 2; ++col) {
        for (int row = 0; row < 4; ++row) {
            const std::uint8_t mask = detail::braille_dots_for_cell(col, row);
            if ((bits & mask) == 0U) {
                continue;
            }
            fill_rect(bmp, ox + static_cast<double>(col) * dw,
                      oy + static_cast<double>(row) * dh, dw, dh, fg, 255);
        }
    }
}

// 方块元素（U+2580–U+259F）：按字符语义画矩形/与背景混色。
// 半块是"上=前景色、下=背景色"——热力图靠它在一个字符里表达两个数值。
bool draw_block_cell(image::Bitmap& bmp, char32_t cp, double x, double y, double cw, double ch,
                     const Rgb& fg, const Rgb& bg, std::uint8_t alpha) {
    const double hw = cw / 2.0;
    const double hh = ch / 2.0;
    const auto mix = [&](double ratio) {
        return Rgb{static_cast<std::uint8_t>(bg.r + (fg.r - bg.r) * ratio),
                    static_cast<std::uint8_t>(bg.g + (fg.g - bg.g) * ratio),
                    static_cast<std::uint8_t>(bg.b + (fg.b - bg.b) * ratio)};
    };
    switch (cp) {
        case 0x2588U:  // █ 实心
            fill_rect(bmp, x, y, cw, ch, fg, alpha);
            return true;
        case 0x2580U:  // ▀ 上半：上=fg，下=bg
            fill_rect(bmp, x, y, cw, ch, bg, alpha);
            fill_rect(bmp, x, y, cw, hh, fg, alpha);
            return true;
        case 0x2584U:  // ▄ 下半：上=bg，下=fg
            fill_rect(bmp, x, y, cw, ch, bg, alpha);
            fill_rect(bmp, x, y + hh, cw, hh, fg, alpha);
            return true;
        case 0x258CU:  // ▌ 左半
            fill_rect(bmp, x, y, cw, ch, bg, alpha);
            fill_rect(bmp, x, y, hw, ch, fg, alpha);
            return true;
        case 0x2590U:  // ▐ 右半
            fill_rect(bmp, x, y, cw, ch, bg, alpha);
            fill_rect(bmp, x + hw, y, hw, ch, fg, alpha);
            return true;
        case 0x2591U:  // ░
            fill_rect(bmp, x, y, cw, ch, mix(0.25), alpha);
            return true;
        case 0x2592U:  // ▒
            fill_rect(bmp, x, y, cw, ch, mix(0.50), alpha);
            return true;
        case 0x2593U:  // ▓
            fill_rect(bmp, x, y, cw, ch, mix(0.75), alpha);
            return true;
        default:
            break;
    }
    // 下八分块 ▁▂▃▅▆▇（▄ 上面已处理）与左八分块 ▏▎▍▋▊▉。
    struct Eighth {
        char32_t cp;
        double frac;
        bool lower;  // true=下八分（底部横条），false=左八分
    };
    static constexpr Eighth kEighths[] = {
        {0x2581U, 1.0 / 8.0, true},  {0x2582U, 2.0 / 8.0, true},  {0x2583U, 3.0 / 8.0, true},
        {0x2585U, 5.0 / 8.0, true},  {0x2586U, 6.0 / 8.0, true},  {0x2587U, 7.0 / 8.0, true},
        {0x258FU, 1.0 / 8.0, false}, {0x258EU, 2.0 / 8.0, false}, {0x258DU, 3.0 / 8.0, false},
        {0x258BU, 5.0 / 8.0, false}, {0x258AU, 6.0 / 8.0, false}, {0x2589U, 7.0 / 8.0, false},
    };
    for (const Eighth& eighth : kEighths) {
        if (cp != eighth.cp) {
            continue;
        }
        fill_rect(bmp, x, y, cw, ch, bg, alpha);
        if (eighth.lower) {
            fill_rect(bmp, x, y + ch * (1.0 - eighth.frac), cw, ch * eighth.frac, fg, alpha);
        } else {
            fill_rect(bmp, x, y, cw * eighth.frac, ch, fg, alpha);
        }
        return true;
    }
    return false;
}

}  // namespace

image::Bitmap render_cells_to_bitmap(const CellBuffer& cells, const CellBitmapOptions& options) {
    const std::uint32_t cols = cells.width();
    const std::uint32_t rows = cells.height();
    const double cw = options.cell_w;
    const double ch = options.cell_h;
    const std::uint32_t width =
        static_cast<std::uint32_t>(std::ceil(cw * static_cast<double>(cols)));
    const std::uint32_t height =
        static_cast<std::uint32_t>(std::ceil(ch * static_cast<double>(rows)));
    image::Bitmap bmp(width, height, 4);
    const std::uint8_t bg_alpha = options.transparent ? 0U : 255U;
    fill_rect(bmp, 0.0, 0.0, static_cast<double>(width), static_cast<double>(height),
              options.background, bg_alpha);

    color::ResolveOptions resolve;
    resolve.depth = ColorDepth::TrueColor;
    resolve.default_fg = options.foreground;
    resolve.default_bg = options.background;

    for (std::uint32_t cy = 0; cy < rows; ++cy) {
        for (std::uint32_t cx = 0; cx < cols; ++cx) {
            const Cell& cell = cells.at(cx, cy);
            if (is_continuation(cell)) {
                continue;  // 宽字符的续接格不产出像素（补空格会盖住字形右半边）
            }
            const color::ResolvedStyle rs = color::resolve_style(cell.style, resolve);
            const double x = cw * static_cast<double>(cx);
            const double y = ch * static_cast<double>(cy);
            if (!rs.bg_transparent) {
                fill_rect(bmp, x, y, cw, ch, rs.bg, bg_alpha);
            }
            const char32_t cp = cell.cp;
            if (cp == U' ') {
                continue;
            }
            if (cp >= 0x2800U && cp <= 0x28FFU) {
                draw_braille(bmp, cp, x, y, cw, ch, rs.fg);
                continue;
            }
            if (draw_block_cell(bmp, cp, x, y, cw, ch, rs.fg, rs.bg, bg_alpha)) {
                continue;
            }
            (void)gui::draw_text(bmp, utf8_of(cp), Point{x, y}, rs.fg, cw, ch);
        }
    }
    return bmp;
}

}  // namespace termchart::gui
