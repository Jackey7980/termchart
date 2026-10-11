#include "termchart/color/palette.hpp"

#include <array>

namespace termchart::color {
namespace {

using PaletteArray = std::array<Rgb, 10>;

// 每个调色板固定 10 色：够覆盖绝大多数系列数量，且"最后一个系列"总能取到可辨色。
constexpr std::array<PaletteArray, kPaletteCount> kPalettes{{
    // Default：深浅背景都可辨、相邻色可区分。
    {Rgb{91, 143, 249}, Rgb{239, 118, 122}, Rgb{86, 197, 154}, Rgb{246, 193, 90},
     Rgb{181, 129, 234}, Rgb{102, 200, 224}, Rgb{232, 143, 199}, Rgb{163, 197, 90},
     Rgb{232, 159, 105}, Rgb{143, 165, 190}},
    // Ocean
    {Rgb{32, 84, 140}, Rgb{58, 132, 190}, Rgb{99, 178, 226}, Rgb{150, 214, 240},
     Rgb{15, 60, 109}, Rgb{90, 162, 186}, Rgb{205, 235, 245}, Rgb{44, 110, 155},
     Rgb{125, 195, 210}, Rgb{12, 42, 78}},
    // Forest
    {Rgb{34, 94, 61}, Rgb{66, 143, 87}, Rgb{122, 189, 122}, Rgb{187, 220, 165},
     Rgb{21, 66, 45}, Rgb{96, 166, 118}, Rgb{222, 238, 209}, Rgb{52, 124, 84},
     Rgb{149, 204, 152}, Rgb{16, 49, 33}},
    // Sunset
    {Rgb{229, 87, 74}, Rgb{246, 149, 79}, Rgb{252, 206, 108}, Rgb{198, 107, 129},
     Rgb{160, 76, 118}, Rgb{120, 63, 107}, Rgb{248, 178, 141}, Rgb{210, 133, 92},
     Rgb{247, 230, 168}, Rgb{143, 78, 84}},
    // Mono：灰度阶梯（打印 / 极简 / 不支持彩色的终端）
    {Rgb{40, 40, 40}, Rgb{72, 72, 72}, Rgb{105, 105, 105}, Rgb{138, 138, 138},
     Rgb{170, 170, 170}, Rgb{192, 192, 192}, Rgb{212, 212, 212}, Rgb{232, 232, 232},
     Rgb{246, 246, 246}, Rgb{255, 255, 255}},
    // HighContrast：色觉障碍友好取向，相邻差异极大
    {Rgb{0, 114, 178}, Rgb{213, 94, 0}, Rgb{0, 158, 115}, Rgb{204, 121, 167},
     Rgb{240, 228, 66}, Rgb{86, 180, 233}, Rgb{230, 159, 0}, Rgb{0, 0, 0},
     Rgb{150, 150, 150}, Rgb{255, 255, 255}},
}};

}  // namespace

std::span<const Rgb> palette(PaletteKind kind) noexcept {
    return std::span<const Rgb>(kPalettes[static_cast<std::size_t>(kind)]);
}

std::span<const Rgb> palette_by_name(std::string_view name) noexcept {
    for (int i = 0; i < kPaletteCount; ++i) {
        const PaletteKind kind = static_cast<PaletteKind>(i);
        if (name == palette_name(kind)) {
            return palette(kind);
        }
    }
    return palette(PaletteKind::Default);
}

Rgb series_color(PaletteKind kind, std::size_t index) noexcept {
    const auto colors = palette(kind);
    return colors[index % colors.size()];
}

std::size_t palette_size(PaletteKind kind) noexcept { return palette(kind).size(); }

const char* palette_name(PaletteKind kind) noexcept {
    switch (kind) {
        case PaletteKind::Default:
            return "default";
        case PaletteKind::Ocean:
            return "ocean";
        case PaletteKind::Forest:
            return "forest";
        case PaletteKind::Sunset:
            return "sunset";
        case PaletteKind::Mono:
            return "mono";
        case PaletteKind::HighContrast:
            return "high-contrast";
    }
    return "default";
}

}  // namespace termchart::color
