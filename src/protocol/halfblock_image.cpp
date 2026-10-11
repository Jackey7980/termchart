// halfblock_image.cpp —— 位图 → 真彩半块字符
#include "termchart/protocol/halfblock_image.hpp"

#include <cstdint>
#include <string>

#include "internal.hpp"

namespace termchart::protocol {

namespace {

void append_truecolor(std::string& out, bool foreground, std::uint8_t r, std::uint8_t g,
                      std::uint8_t b) {
    out += kEsc;
    out += '[';
    out += foreground ? "38;2;" : "48;2;";
    out += std::to_string(r);
    out += ';';
    out += std::to_string(g);
    out += ';';
    out += std::to_string(b);
    out += 'm';
}

}  // namespace

std::string HalfBlockProtocol::encode(const image::Bitmap& bitmap,
                                      const ProtocolOptions& opts) const {
    (void)opts;
    const std::uint32_t w = bitmap.width();
    const std::uint32_t h = bitmap.height();
    if (w == 0 || h == 0 || bitmap.empty()) {
        return {};
    }

    const std::uint32_t rows = (h + 1) / 2;
    std::string out;
    for (std::uint32_t row = 0; row < rows; ++row) {
        const std::uint32_t y_top = row * 2;
        const std::uint32_t y_bot = y_top + 1;
        for (std::uint32_t x = 0; x < w; ++x) {
            append_truecolor(out, true, bitmap.at(x, y_top, 0), bitmap.at(x, y_top, 1),
                             bitmap.at(x, y_top, 2));
            if (y_bot < h) {
                append_truecolor(out, false, bitmap.at(x, y_bot, 0), bitmap.at(x, y_bot, 1),
                                 bitmap.at(x, y_bot, 2));
            } else {
                append_truecolor(out, false, 0, 0, 0);  // 奇数高度：末行下半填黑
            }
            out += "\u2580";  // ▀
        }
        out += kEsc;
        out += "[0m";
        if (row + 1 < rows) {
            out += '\n';
        }
    }
    return out;
}

}  // namespace termchart::protocol
