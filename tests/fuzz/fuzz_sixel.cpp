// fuzz_sixel.cpp —— sixel 编码的边界（规格 fuzz 目标之一）
//
// 输入是"字节流"，但 sixel 编码器吃的是 `image::Bitmap`，所以这里先把字节**确定性地**
// 解释成一张小位图（尺寸由前两字节决定，上限 16×16），再编码。
// 关注点：调色板量化（颜色数远超 sixel 的 256 色上限时）、RLE 行程、以及
// **零尺寸/带 alpha 的边界**（编码器必须给出合法或空的结果，不能越界读像素）。
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "fuzz_common.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart/protocol/sixel.hpp"

namespace {

termchart::image::Bitmap bitmap_from_bytes(const std::uint8_t* data, std::size_t size) {
    const std::uint32_t width = 1u + (size > 0 ? data[0] % 16u : 0u);
    const std::uint32_t height = 1u + (size > 1 ? data[1] % 16u : 0u);
    const std::uint8_t channels = (size > 2 && (data[2] & 1u) != 0u) ? 4u : 3u;

    termchart::image::Bitmap bitmap(width, height, channels);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            for (std::uint8_t ch = 0; ch < channels; ++ch) {
                const std::size_t at =
                    (static_cast<std::size_t>(y) * width + x) * channels + ch;
                const std::uint8_t value = at < size ? data[at] : static_cast<std::uint8_t>(0);
                bitmap.set(x, y, ch, value);
            }
        }
    }
    return bitmap;
}

// 一条 sixel 序列必须**整条**合法：DCS 引导 `ESC P` 开头、ST `ESC \` 结尾。
// 半截序列比崩溃更糟：它会把终端留在"等待 DCS 结束"的状态里，后续所有输出都被
// 当成 sixel 数据吞掉。所以这里不是"看看有没有 ESC P"，而是直接断言两端。
void require_well_formed(const std::string& encoded) {
    if (encoded.empty()) {
        return;  // 空位图允许返回空串（调用方自己判空）
    }
    const bool has_dcs_prefix = encoded.rfind("\x1bP", 0) == 0;
    const bool has_st_terminator =
        encoded.size() >= 2 && encoded.compare(encoded.size() - 2, 2, "\x1b\\") == 0;
    if (!has_dcs_prefix || !has_st_terminator) {
        std::abort();
    }
}

void target(const std::uint8_t* data, std::size_t size) {
    const termchart::image::Bitmap bitmap = bitmap_from_bytes(data, size);
    require_well_formed(termchart::protocol::SixelProtocol{}.encode(bitmap));

    // 零尺寸位图：编码器不得崩，且同样不能产出"半个序列"。
    termchart::image::Bitmap empty;
    require_well_formed(termchart::protocol::SixelProtocol{}.encode(empty));
}

}  // namespace

TERMCHART_FUZZ_MAIN(target)
