#include "termchart/color/quantize.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>  // std::abs(int)：MinGW 靠传递包含能过，Emscripten/libc++ 必须显式包含

namespace termchart::color {
namespace {

// 标准 16 色（索引顺序即 ANSI 编号）：0..7 普通，8..15 高亮。
constexpr std::array<Rgb, 16> kAnsi16Palette{{
    Rgb{0, 0, 0},       Rgb{128, 0, 0},     Rgb{0, 128, 0},     Rgb{128, 128, 0},
    Rgb{0, 0, 128},     Rgb{128, 0, 128},   Rgb{0, 128, 128},   Rgb{192, 192, 192},
    Rgb{128, 128, 128}, Rgb{255, 0, 0},     Rgb{0, 255, 0},     Rgb{255, 255, 0},
    Rgb{0, 0, 255},     Rgb{255, 0, 255},   Rgb{0, 255, 255},   Rgb{255, 255, 255},
}};

double squared_distance(Rgb a, Rgb b) noexcept {
    const double dr = static_cast<double>(a.r) - static_cast<double>(b.r);
    const double dg = static_cast<double>(a.g) - static_cast<double>(b.g);
    const double db = static_cast<double>(a.b) - static_cast<double>(b.b);
    // Rec.709 加权：纯欧氏距离会让蓝色偏差过于显眼。
    return 0.2126 * dr * dr + 0.7152 * dg * dg + 0.0722 * db * db;
}

}  // namespace

std::uint8_t to_index_256(Rgb color) noexcept {
    const bool greyscale = color.r == color.g && color.g == color.b;
    if (greyscale) {
        if (color.r < 8) {
            return 16;
        }
        if (color.r > 248) {
            return 231;
        }
        return static_cast<std::uint8_t>(232 + (static_cast<int>(color.r) - 8) / 10);
    }
    const int levels[6] = {0, 95, 135, 175, 215, 255};
    int r_index = 0;
    int g_index = 0;
    int b_index = 0;
    for (int i = 1; i < 6; ++i) {
        if (std::abs(static_cast<int>(color.r) - levels[i]) <
            std::abs(static_cast<int>(color.r) - levels[r_index])) {
            r_index = i;
        }
        if (std::abs(static_cast<int>(color.g) - levels[i]) <
            std::abs(static_cast<int>(color.g) - levels[g_index])) {
            g_index = i;
        }
        if (std::abs(static_cast<int>(color.b) - levels[i]) <
            std::abs(static_cast<int>(color.b) - levels[b_index])) {
            b_index = i;
        }
    }
    return static_cast<std::uint8_t>(16 + 36 * r_index + 6 * g_index + b_index);
}

std::uint8_t to_index_16(Rgb color) noexcept {
    std::size_t best = 0;
    double best_distance = squared_distance(color, kAnsi16Palette[0]);
    for (std::size_t i = 1; i < kAnsi16Palette.size(); ++i) {
        const double d = squared_distance(color, kAnsi16Palette[i]);
        if (d < best_distance) {
            best_distance = d;
            best = i;
        }
    }
    return static_cast<std::uint8_t>(best);
}

Rgb from_index_256(std::uint8_t index) noexcept {
    if (index < 16) {
        return kAnsi16Palette[index];
    }
    if (index < 232) {
        const int value = index - 16;
        const int levels[6] = {0, 95, 135, 175, 215, 255};
        return Rgb{static_cast<std::uint8_t>(levels[value / 36]),
                   static_cast<std::uint8_t>(levels[(value / 6) % 6]),
                   static_cast<std::uint8_t>(levels[value % 6])};
    }
    const std::uint8_t level = static_cast<std::uint8_t>(8 + (index - 232) * 10);
    return Rgb{level, level, level};
}

Rgb from_index_16(std::uint8_t index) noexcept { return kAnsi16Palette[index & 0x0Fu]; }

Color degrade(Color color, ColorDepth depth) noexcept {
    switch (color.kind()) {
        case ColorKind::None:
            return color;
        case ColorKind::Ansi16:
            return depth >= ColorDepth::Ansi16 ? color : Color::none();
        case ColorKind::Indexed:
            if (depth >= ColorDepth::Ansi256) {
                return color;
            }
            return depth >= ColorDepth::Ansi16
                       ? Color::ansi16(to_index_16(from_index_256(color.index())))
                       : Color::none();
        case ColorKind::Rgb:
            break;
    }
    // ColorKind::Rgb
    switch (depth) {
        case ColorDepth::TrueColor:
            return color;
        case ColorDepth::Ansi256:
            return Color::indexed(to_index_256(color.rgb()));
        case ColorDepth::Ansi16:
            return Color::ansi16(to_index_16(color.rgb()));
        case ColorDepth::None:
        default:
            return Color::none();
    }
}

Style degrade(Style style, ColorDepth depth) noexcept {
    Style out = style;
    out.set_fg(degrade(style.fg(), depth));
    out.set_bg(degrade(style.bg(), depth));
    return out;
}

}  // namespace termchart::color
