// named_colors.hpp —— CSS 颜色名查找表（header-only）
//
// 用途：让用户能写 "#2f6f4f" 之外的 "seagreen"，也让示例代码更可读。
// 表按名字升序存放，查询走二分——**因此顺序与内容是契约**：新增必须保持有序
// （重新生成见 tools/gen_colormap.py 的同名策略）。
//
// 当前为 **CSS Color 命名色全集 148 色**（含 gray/grey 两套拼法与 rebeccapurple，
// 即 CSS Color Module Level 4 的完整列表，不含 `transparent`/`currentcolor`
// 这两个关键字——它们不是颜色值，由 Color 侧单独处理）。
// 查不到的名字由 Color 侧回落到 "不是颜色名" 而非魔数。
//
// 契约由 `tests/unit/color/test_color.cpp` 守住：表必须严格升序、无重复、
// 数量等于 kNamedColorCount、抽查若干色值与 CSS 规范一致。
#ifndef TERMCHART_COLOR_NAMED_COLORS_HPP
#define TERMCHART_COLOR_NAMED_COLORS_HPP

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

#include "termchart/core/rgb.hpp"

namespace termchart::color {

struct NamedColorEntry {
    std::string_view name;
    Rgb value;
};

// CSS 命名色全集的大小（148）。二分查找的上界与容量都取自它，不要让二者各写一份数字。
inline constexpr std::size_t kNamedColorCount = 148;

// clang-format off
inline constexpr std::array<NamedColorEntry, kNamedColorCount> kNamedColors{{
    {"aliceblue", Rgb{240, 248, 255}},
    {"antiquewhite", Rgb{250, 235, 215}},
    {"aqua", Rgb{0, 255, 255}},
    {"aquamarine", Rgb{127, 255, 212}},
    {"azure", Rgb{240, 255, 255}},
    {"beige", Rgb{245, 245, 220}},
    {"bisque", Rgb{255, 228, 196}},
    {"black", Rgb{0, 0, 0}},
    {"blanchedalmond", Rgb{255, 235, 205}},
    {"blue", Rgb{0, 0, 255}},
    {"blueviolet", Rgb{138, 43, 226}},
    {"brown", Rgb{165, 42, 42}},
    {"burlywood", Rgb{222, 184, 135}},
    {"cadetblue", Rgb{95, 158, 160}},
    {"chartreuse", Rgb{127, 255, 0}},
    {"chocolate", Rgb{210, 105, 30}},
    {"coral", Rgb{255, 127, 80}},
    {"cornflowerblue", Rgb{100, 149, 237}},
    {"cornsilk", Rgb{255, 248, 220}},
    {"crimson", Rgb{220, 20, 60}},
    {"cyan", Rgb{0, 255, 255}},
    {"darkblue", Rgb{0, 0, 139}},
    {"darkcyan", Rgb{0, 139, 139}},
    {"darkgoldenrod", Rgb{184, 134, 11}},
    {"darkgray", Rgb{169, 169, 169}},
    {"darkgreen", Rgb{0, 100, 0}},
    {"darkgrey", Rgb{169, 169, 169}},
    {"darkkhaki", Rgb{189, 183, 107}},
    {"darkmagenta", Rgb{139, 0, 139}},
    {"darkolivegreen", Rgb{85, 107, 47}},
    {"darkorange", Rgb{255, 140, 0}},
    {"darkorchid", Rgb{153, 50, 204}},
    {"darkred", Rgb{139, 0, 0}},
    {"darksalmon", Rgb{233, 150, 122}},
    {"darkseagreen", Rgb{143, 188, 143}},
    {"darkslateblue", Rgb{72, 61, 139}},
    {"darkslategray", Rgb{47, 79, 79}},
    {"darkslategrey", Rgb{47, 79, 79}},
    {"darkturquoise", Rgb{0, 206, 209}},
    {"darkviolet", Rgb{148, 0, 211}},
    {"deeppink", Rgb{255, 20, 147}},
    {"deepskyblue", Rgb{0, 191, 255}},
    {"dimgray", Rgb{105, 105, 105}},
    {"dimgrey", Rgb{105, 105, 105}},
    {"dodgerblue", Rgb{30, 144, 255}},
    {"firebrick", Rgb{178, 34, 34}},
    {"floralwhite", Rgb{255, 250, 240}},
    {"forestgreen", Rgb{34, 139, 34}},
    {"fuchsia", Rgb{255, 0, 255}},
    {"gainsboro", Rgb{220, 220, 220}},
    {"ghostwhite", Rgb{248, 248, 255}},
    {"gold", Rgb{255, 215, 0}},
    {"goldenrod", Rgb{218, 165, 32}},
    {"gray", Rgb{128, 128, 128}},
    {"green", Rgb{0, 128, 0}},
    {"greenyellow", Rgb{173, 255, 47}},
    {"grey", Rgb{128, 128, 128}},
    {"honeydew", Rgb{240, 255, 240}},
    {"hotpink", Rgb{255, 105, 180}},
    {"indianred", Rgb{205, 92, 92}},
    {"indigo", Rgb{75, 0, 130}},
    {"ivory", Rgb{255, 255, 240}},
    {"khaki", Rgb{240, 230, 140}},
    {"lavender", Rgb{230, 230, 250}},
    {"lavenderblush", Rgb{255, 240, 245}},
    {"lawngreen", Rgb{124, 252, 0}},
    {"lemonchiffon", Rgb{255, 250, 205}},
    {"lightblue", Rgb{173, 216, 230}},
    {"lightcoral", Rgb{240, 128, 128}},
    {"lightcyan", Rgb{224, 255, 255}},
    {"lightgoldenrodyellow", Rgb{250, 250, 210}},
    {"lightgray", Rgb{211, 211, 211}},
    {"lightgreen", Rgb{144, 238, 144}},
    {"lightgrey", Rgb{211, 211, 211}},
    {"lightpink", Rgb{255, 182, 193}},
    {"lightsalmon", Rgb{255, 160, 122}},
    {"lightseagreen", Rgb{32, 178, 170}},
    {"lightskyblue", Rgb{135, 206, 250}},
    {"lightslategray", Rgb{119, 136, 153}},
    {"lightslategrey", Rgb{119, 136, 153}},
    {"lightsteelblue", Rgb{176, 196, 222}},
    {"lightyellow", Rgb{255, 255, 224}},
    {"lime", Rgb{0, 255, 0}},
    {"limegreen", Rgb{50, 205, 50}},
    {"linen", Rgb{250, 240, 230}},
    {"magenta", Rgb{255, 0, 255}},
    {"maroon", Rgb{128, 0, 0}},
    {"mediumaquamarine", Rgb{102, 205, 170}},
    {"mediumblue", Rgb{0, 0, 205}},
    {"mediumorchid", Rgb{186, 85, 211}},
    {"mediumpurple", Rgb{147, 112, 219}},
    {"mediumseagreen", Rgb{60, 179, 113}},
    {"mediumslateblue", Rgb{123, 104, 238}},
    {"mediumspringgreen", Rgb{0, 250, 154}},
    {"mediumturquoise", Rgb{72, 209, 204}},
    {"mediumvioletred", Rgb{199, 21, 133}},
    {"midnightblue", Rgb{25, 25, 112}},
    {"mintcream", Rgb{245, 255, 250}},
    {"mistyrose", Rgb{255, 228, 225}},
    {"moccasin", Rgb{255, 228, 181}},
    {"navajowhite", Rgb{255, 222, 173}},
    {"navy", Rgb{0, 0, 128}},
    {"oldlace", Rgb{253, 245, 230}},
    {"olive", Rgb{128, 128, 0}},
    {"olivedrab", Rgb{107, 142, 35}},
    {"orange", Rgb{255, 165, 0}},
    {"orangered", Rgb{255, 69, 0}},
    {"orchid", Rgb{218, 112, 214}},
    {"palegoldenrod", Rgb{238, 232, 170}},
    {"palegreen", Rgb{152, 251, 152}},
    {"paleturquoise", Rgb{175, 238, 238}},
    {"palevioletred", Rgb{219, 112, 147}},
    {"papayawhip", Rgb{255, 239, 213}},
    {"peachpuff", Rgb{255, 218, 185}},
    {"peru", Rgb{205, 133, 63}},
    {"pink", Rgb{255, 192, 203}},
    {"plum", Rgb{221, 160, 221}},
    {"powderblue", Rgb{176, 224, 230}},
    {"purple", Rgb{128, 0, 128}},
    {"rebeccapurple", Rgb{102, 51, 153}},
    {"red", Rgb{255, 0, 0}},
    {"rosybrown", Rgb{188, 143, 143}},
    {"royalblue", Rgb{65, 105, 225}},
    {"saddlebrown", Rgb{139, 69, 19}},
    {"salmon", Rgb{250, 128, 114}},
    {"sandybrown", Rgb{244, 164, 96}},
    {"seagreen", Rgb{46, 139, 87}},
    {"seashell", Rgb{255, 245, 238}},
    {"sienna", Rgb{160, 82, 45}},
    {"silver", Rgb{192, 192, 192}},
    {"skyblue", Rgb{135, 206, 235}},
    {"slateblue", Rgb{106, 90, 205}},
    {"slategray", Rgb{112, 128, 144}},
    {"slategrey", Rgb{112, 128, 144}},
    {"snow", Rgb{255, 250, 250}},
    {"springgreen", Rgb{0, 255, 127}},
    {"steelblue", Rgb{70, 130, 180}},
    {"tan", Rgb{210, 180, 140}},
    {"teal", Rgb{0, 128, 128}},
    {"thistle", Rgb{216, 191, 216}},
    {"tomato", Rgb{255, 99, 71}},
    {"turquoise", Rgb{64, 224, 208}},
    {"violet", Rgb{238, 130, 238}},
    {"wheat", Rgb{245, 222, 179}},
    {"white", Rgb{255, 255, 255}},
    {"whitesmoke", Rgb{245, 245, 245}},
    {"yellow", Rgb{255, 255, 0}},
    {"yellowgreen", Rgb{154, 205, 50}},
}};
// clang-format on

// 查表：名字必须全小写且完全匹配，否则返回 nullopt。
constexpr std::optional<Rgb> lookup_named_color(std::string_view name) noexcept {
    std::size_t lo = 0;
    std::size_t hi = kNamedColors.size();
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (kNamedColors[mid].name < name) {
            lo = mid + 1;
        } else if (name < kNamedColors[mid].name) {
            hi = mid;
        } else {
            return kNamedColors[mid].value;
        }
    }
    return std::nullopt;
}

}  // namespace termchart::color

#endif  // TERMCHART_COLOR_NAMED_COLORS_HPP
