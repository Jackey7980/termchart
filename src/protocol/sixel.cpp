// sixel.cpp —— Sixel 编码：xterm-256 量化 + 调色板定义 + 6 行分带 + 游程压缩
#include "termchart/protocol/sixel.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/color/quantize.hpp"
#include "termchart/core/rgb.hpp"
#include "internal.hpp"

namespace termchart::protocol {

namespace {

// 六象素游程压缩：连续 >= 4 个相同字符写成 !<count><char>。
std::string rle_compress(const std::string& data) {
    std::string out;
    std::size_t i = 0;
    while (i < data.size()) {
        std::size_t j = i;
        while (j < data.size() && data[j] == data[i]) {
            ++j;
        }
        const std::size_t run = j - i;
        if (run >= 4) {
            out += '!';
            out += std::to_string(run);
            out += data[i];
        } else {
            out.append(run, data[i]);
        }
        i = j;
    }
    return out;
}

}  // namespace

std::string SixelProtocol::encode(const image::Bitmap& bitmap,
                                  const ProtocolOptions& opts) const {
    const std::uint32_t w = bitmap.width();
    const std::uint32_t h = bitmap.height();
    if (w == 0 || h == 0 || bitmap.empty()) {
        return {};
    }

    // 1) 逐像素量化到 xterm-256，并记录用到的索引。
    std::vector<std::uint8_t> index(static_cast<std::size_t>(w) * h, 0);
    bool used[256] = {};
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const Rgb rgb{bitmap.at(x, y, 0), bitmap.at(x, y, 1), bitmap.at(x, y, 2)};
            const std::uint8_t i = color::to_index_256(rgb);
            index[static_cast<std::size_t>(y) * w + x] = i;
            used[i] = true;
        }
    }

    // 2) DCS 引入 + 光栅属性。
    std::string out;
    out += kEsc;
    out += "P0;0;0q";
    out += "\"1;1;";
    out += std::to_string(w);
    out += ';';
    out += std::to_string(h);

    // 3) 调色板定义（只定义用到的颜色）。
    for (int i = 0; i < 256; ++i) {
        if (!used[i]) {
            continue;
        }
        const Rgb c = color::from_index_256(static_cast<std::uint8_t>(i));
        out += '#';
        out += std::to_string(i);
        out += ";2;";
        out += std::to_string(c.r);
        out += ';';
        out += std::to_string(c.g);
        out += ';';
        out += std::to_string(c.b);
    }

    // 4) 每 6 行一带；带内按颜色逐个输出，颜色间用 '$'，带间用 '-'。
    const std::uint32_t bands = (h + 5) / 6;
    for (std::uint32_t band = 0; band < bands; ++band) {
        const std::uint32_t y0 = band * 6;
        bool first_in_band = true;
        for (int i = 0; i < 256; ++i) {
            if (!used[i]) {
                continue;
            }
            std::string line;
            line.reserve(w);
            bool appears = false;
            for (std::uint32_t x = 0; x < w; ++x) {
                std::uint8_t mask = 0;
                for (std::uint32_t k = 0; k < 6; ++k) {
                    const std::uint32_t y = y0 + k;
                    if (y < h && index[static_cast<std::size_t>(y) * w + x] == i) {
                        mask |= static_cast<std::uint8_t>(1u << k);
                    }
                }
                if (mask != 0) {
                    appears = true;
                }
                line += static_cast<char>(63 + mask);
            }
            if (!appears) {
                continue;
            }
            if (!first_in_band) {
                out += '$';
            }
            out += '#';
            out += std::to_string(i);
            out += opts.rle ? rle_compress(line) : line;
            first_in_band = false;
        }
        if (band + 1 < bands) {
            out += '-';
        }
    }

    out += kEsc;
    out += '\\';
    return out;
}

}  // namespace termchart::protocol
