// named_colors.hpp —— CSS 颜色名查找表（header-only）
//
// 用途：让用户能写 "#2f6f4f" 之外的 "seagreen"，也让示例代码更可读。
// 表按名字升序存放，查询走二分——**因此顺序与内容是契约**：新增必须保持有序
// （重新生成见 tools/gen_colormap.py 的同名策略）。
//
// 当前为常用子集（68 个），完整 148 色 CSS 列表在阶段 7 补齐；
// 查不到的名字由 Color 侧回落到 "不是颜色名" 而非魔数。
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

// clang-format off
inline constexpr std::array<NamedColorEntry, 68> kNamedColors{{
    {"aliceblue", Rgb{240, 248, 255}},    {"antiquewhite", Rgb{250, 235, 215}},
    {"aqua", Rgb{0, 255, 255}},           {"aquamarine", Rgb{127, 255, 212}},
    {"azure", Rgb{240, 255, 255}},        {"beige", Rgb{245, 245, 220}},
    {"black", Rgb{0, 0, 0}},              {"blanchedalmond", Rgb{255, 235, 205}},
    {"blue", Rgb{0, 0, 255}},             {"blueviolet", Rgb{138, 43, 226}},
    {"brown", Rgb{165, 42, 42}},          {"burlywood", Rgb{222, 184, 135}},
    {"cadetblue", Rgb{95, 158, 160}},     {"chartreuse", Rgb{127, 255, 0}},
    {"coral", Rgb{255, 127, 80}},         {"cornflowerblue", Rgb{100, 149, 237}},
    {"crimson", Rgb{220, 20, 60}},        {"cyan", Rgb{0, 255, 255}},
    {"darkcyan", Rgb{0, 139, 139}},       {"darkgray", Rgb{169, 169, 169}},
    {"darkgreen", Rgb{0, 100, 0}},        {"darkorange", Rgb{255, 140, 0}},
    {"darkred", Rgb{139, 0, 0}},          {"darkslategray", Rgb{47, 79, 79}},
    {"deepskyblue", Rgb{0, 191, 255}},    {"dodgerblue", Rgb{30, 144, 255}},
    {"firebrick", Rgb{178, 34, 34}},      {"forestgreen", Rgb{34, 139, 34}},
    {"gold", Rgb{255, 215, 0}},           {"goldenrod", Rgb{218, 165, 32}},
    {"gray", Rgb{128, 128, 128}},         {"green", Rgb{0, 128, 0}},
    {"greenyellow", Rgb{173, 255, 47}},   {"hotpink", Rgb{255, 105, 180}},
    {"indigo", Rgb{75, 0, 130}},          {"khaki", Rgb{240, 230, 140}},
    {"lavender", Rgb{230, 230, 250}},     {"lightblue", Rgb{173, 216, 230}},
    {"lightgray", Rgb{211, 211, 211}},    {"lime", Rgb{0, 255, 0}},
    {"magenta", Rgb{255, 0, 255}},        {"maroon", Rgb{128, 0, 0}},
    {"navy", Rgb{0, 0, 128}},             {"olive", Rgb{128, 128, 0}},
    {"orange", Rgb{255, 165, 0}},         {"orangered", Rgb{255, 69, 0}},
    {"orchid", Rgb{218, 112, 214}},       {"salmon", Rgb{250, 128, 114}},
    {"sandybrown", Rgb{244, 164, 96}},    {"seagreen", Rgb{46, 139, 87}},
    {"silver", Rgb{192, 192, 192}},       {"skyblue", Rgb{135, 206, 235}},
    {"slateblue", Rgb{106, 90, 205}},     {"slategray", Rgb{112, 128, 144}},
    {"snow", Rgb{255, 250, 250}},         {"springgreen", Rgb{0, 255, 127}},
    {"steelblue", Rgb{70, 130, 180}},     {"tan", Rgb{210, 180, 140}},
    {"teal", Rgb{0, 128, 128}},           {"thistle", Rgb{216, 191, 216}},
    {"tomato", Rgb{255, 99, 71}},         {"turquoise", Rgb{64, 224, 208}},
    {"violet", Rgb{238, 130, 238}},       {"wheat", Rgb{245, 222, 179}},
    {"white", Rgb{255, 255, 255}},        {"whitesmoke", Rgb{245, 245, 245}},
    {"yellow", Rgb{255, 255, 0}},         {"yellowgreen", Rgb{154, 205, 50}},
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
