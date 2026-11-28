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

// 24-bit 打包值，只用于"这两个颜色是否相同"的判定。
constexpr int pack_rgb(int r, int g, int b) { return (r << 16) | (g << 8) | b; }

}  // namespace

std::string HalfBlockProtocol::encode(const image::Bitmap& bitmap,
                                      const ProtocolOptions& opts) const {
    (void)opts;  // 半块没有压缩/调色板参数：真彩直出，靠差分 SGR 控制体积
    const std::uint32_t w = bitmap.width();
    const std::uint32_t h = bitmap.height();
    if (w == 0 || h == 0 || bitmap.empty()) {
        return {};
    }

    const std::uint32_t rows = (h + 1) / 2;
    std::string out;
    // 每格 3 字节 ▀ + 少量差分 SGR；大面积同色背景时这是压倒性的大头。
    out.reserve(static_cast<std::size_t>(w) * rows * 4 + 64);
    for (std::uint32_t row = 0; row < rows; ++row) {
        const std::uint32_t y_top = row * 2;
        const std::uint32_t y_bot = y_top + 1;
        // 每行从"未知色"起步：行内只发变化的 SGR，行末统一复位，于是每一行都是
        // 自包含的（调用方可以逐行打印/落盘而不丢颜色）。
        int prev_fg = -1;
        int prev_bg = -1;
        for (std::uint32_t x = 0; x < w; ++x) {
            const std::uint8_t r = bitmap.at(x, y_top, 0);
            const std::uint8_t g = bitmap.at(x, y_top, 1);
            const std::uint8_t b = bitmap.at(x, y_top, 2);
            const int fg = pack_rgb(r, g, b);
            if (fg != prev_fg) {
                append_truecolor(out, true, r, g, b);
                prev_fg = fg;
            }
            if (y_bot < h) {
                const std::uint8_t br = bitmap.at(x, y_bot, 0);
                const std::uint8_t bg = bitmap.at(x, y_bot, 1);
                const std::uint8_t bb = bitmap.at(x, y_bot, 2);
                const int bgv = pack_rgb(br, bg, bb);
                if (bgv != prev_bg) {
                    append_truecolor(out, false, br, bg, bb);
                    prev_bg = bgv;
                }
            } else if (prev_bg != 0) {
                append_truecolor(out, false, 0, 0, 0);  // 奇数高度：末行下半填黑
                prev_bg = 0;
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
