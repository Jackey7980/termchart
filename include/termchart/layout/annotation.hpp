// annotation.hpp —— 参考线、区域高亮、标注
//
// 三者都是"叠在数据之上"的辅助标记，共用 `PlotArea` 的数据 ↔ 格映射，并且一律
// **裁剪在绘图区内**（不会画到轴标签上，也不会溢出画布）。
//
// 区域高亮的语义刻意做成"只改背景色"：终端里"高亮一个区间"应当是让原有数据
// 换底色，而不是把数据涂掉。需要填充字符时才用 fill_char。
#ifndef TERMCHART_LAYOUT_ANNOTATION_HPP
#define TERMCHART_LAYOUT_ANNOTATION_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/box.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::layout {

enum class Orientation {
    Horizontal,  // 水平：跨整个绘图区宽度，用 y 值定位
    Vertical,    // 垂直：跨整个绘图区高度，用 x 值定位
};

struct ReferenceLine {
    Orientation orientation = Orientation::Horizontal;
    double value = 0.0;             // 数据坐标：水平线是 y 值，垂直线是 x 值
    char32_t cp = U'\u2504';        // 默认 ┄（虚线，不喧宾夺主）
    Style style{};
};

struct HighlightBand {
    Orientation orientation = Orientation::Horizontal;
    double from = 0.0;  // 数据坐标（含）
    double to = 0.0;    // 数据坐标（含）；顺序可反
    Color color = Color::ansi16(8);
    // 0 表示不改字符、只改背景色（保留数据内容）；非 0 则用该字符填充整条带。
    char32_t fill_char = U'\0';
};

struct Annotation {
    Point at{};  // 数据坐标锚点
    std::string text;
    Style style{};
};

// 返回 false 表示取值落在数据域之外，或算出的行/列不在绘图区内（此时不画）。
[[nodiscard]] TERMCHART_API bool draw_reference_line(canvas::Canvas& canvas,
                                                     const PlotArea& area,
                                                     const ReferenceLine& line);

// 返回被改动的格数（已裁剪到绘图区；宽字符续接格跳过，由头部统一着色）。
[[nodiscard]] TERMCHART_API std::size_t draw_highlight_band(canvas::Canvas& canvas,
                                                            const PlotArea& area,
                                                            const HighlightBand& band);

// 标注落点用通用避让（layout/label_avoid）在锚点四周找位，并限制在绘图区内。
// 放不下时返回 false 且**什么都不画** —— 宁可少一个标注，也不覆盖已有内容。
[[nodiscard]] TERMCHART_API bool draw_annotation(canvas::Canvas& canvas,
                                                 const PlotArea& area,
                                                 const Annotation& annotation,
                                                 const std::vector<Rect>& occupied = {});

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_ANNOTATION_HPP
