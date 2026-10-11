// fuzz_colormap.cpp —— 色表采样/命名色/量化的边界（规格 fuzz 目标之一）
//
// 关注点：三处最容易出事的地方 ——
//   1) 采样参数越界（NaN、±inf、负值、远大于 1）必须被钳住而不是算出未定义值；
//   2) `stops(kind, count)` 的 count 由输入驱动时**必须设上限**（否则一个字节就能
//      让 fuzz 去申请几百 MB）；
//   3) 量化往返（rgb → 索引 → rgb）不得越界读调色板。
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "fuzz_common.hpp"
#include "termchart/color/colormap.hpp"
#include "termchart/color/named_colors.hpp"
#include "termchart/color/quantize.hpp"
#include "termchart/core/color_depth.hpp"
#include "termchart/core/style.hpp"

namespace {

using termchart::Color;
using termchart::ColorDepth;
using termchart::Rgb;
using termchart::Style;

void target(const std::uint8_t* data, std::size_t size) {
    if (size == 0) {
        return;
    }
    const auto input = std::string_view(reinterpret_cast<const char*>(data), size);

    // **绝不能用裸下标读 data[1]/data[2]**：输入可能是 1~2 字节。
    // 之前这么写"看起来没事"只是因为输入来自 std::string 的 SSO 内联缓冲；
    // 一旦按 libFuzzer 那样喂精确长度的堆缓冲，就是 heap-buffer-overflow。
    const auto byte_at = [data, size](std::size_t index) {
        return data[index < size ? index : size - 1];
    };

    // 1) 命名色：任意字节串（含非 UTF-8、超长）都必须安全返回 optional。
    (void)termchart::color::lookup_named_color(input);
    (void)termchart::color::lookup_named_color(input.substr(0, 64));

    // 2) 色表：种类与参数都由输入决定，t 覆盖正常值、越界值与 NaN。
    const int kinds = static_cast<int>(termchart::color::kColormapCount);
    const auto kind = static_cast<termchart::color::ColormapKind>(byte_at(0) % kinds);
    const double raw_t = static_cast<double>(byte_at(1)) / 255.0;
    for (const double t : {raw_t, -raw_t, raw_t + 1.0, raw_t * 100.0, 0.0, 1.0,
                           std::nan("")}) {
        (void)termchart::color::sample(kind, t);
    }
    (void)termchart::color::colormap_name(kind);
    // count 必须夹住：这里取 0..512，绝不让输入直接决定分配大小。
    const std::size_t count = static_cast<std::size_t>(byte_at(2)) * 2u;
    const auto table = termchart::color::stops(kind, count);
    if (!table.empty()) {
        (void)table.front();
        (void)table.back();
    }

    // 3) 量化往返：任意 rgb → 索引 → rgb，再交给 degrade。
    const Rgb rgb{byte_at(0), byte_at(1), byte_at(2)};
    const std::uint8_t index256 = termchart::color::to_index_256(rgb);
    const std::uint8_t index16 = termchart::color::to_index_16(rgb);
    (void)termchart::color::from_index_256(index256);
    (void)termchart::color::from_index_16(index16);

    const int depths = 4;
    const auto depth = static_cast<ColorDepth>(byte_at(0) % depths);
    (void)termchart::color::degrade(Color::rgb(rgb.r, rgb.g, rgb.b), depth);
    (void)termchart::color::degrade(Color::indexed(index256), depth);
    (void)termchart::color::degrade(Color::ansi16(static_cast<std::uint8_t>(index16 & 0x0F)),
                                   depth);
    (void)termchart::color::degrade(Color::none(), depth);

    Style style;
    style.set_fg(Color::rgb(rgb.r, rgb.g, rgb.b)).set_bg(Color::indexed(index256));
    (void)termchart::color::degrade(style, depth);
}

}  // namespace

TERMCHART_FUZZ_MAIN(target)
