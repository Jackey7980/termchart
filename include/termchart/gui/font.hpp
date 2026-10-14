// font.hpp —— GUI 位图字体的唯一文本出口
//
// 为什么自带点阵字体、而不是接系统字体（TTF / fontconfig / DirectWrite）？
// 1. 零外部依赖是本仓库的硬约束：接系统字体意味着每个平台一套枚举、加载与
//    回退代码，还要为"字体缺失"设计错误路径——与全库"零强制依赖、缺失只
//    降级不失败"的约定直接冲突。
// 2. 终端图表的文本是**等宽语义**：列数由 util 的显示宽度决定（CJK 占 2 列），
//    每格 advance 恒等于 cell_w。点阵字形天然等宽步进，量宽是纯乘法，不需要
//    shaping / hinting，也不存在字距把布局挤歪的问题。
// 3. Scene 不建模排版（没有字体、字形、行盒的概念），TextOp 只带一段 UTF-8
//    与一个锚点；把排版塞进 Scene 属于层次污染。这里把"一格一字符"渲染成
//    像素点阵，是与该模型自洽的最简文本出口。
//
// 字形是 5×7 列位点阵（数据与位序约定见 font.cpp 的 kFont5x7），渲染时映射进
// cell_w×cell_h 的整数格：字形画在前 5/6 格宽、前 7/8 格高（末列/末行留作
// 字距），保证 advance == cell_w 整数对齐。缺字码点画"虚线占位框"而不是
// 留白——中文标签掉成空白比掉成虚框更难排查，可见且可度量是底线。
#ifndef TERMCHART_GUI_FONT_HPP
#define TERMCHART_GUI_FONT_HPP
#include <string_view>
#include "termchart/core/point.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/export.hpp"
#include "termchart/image/bitmap.hpp"
namespace termchart::gui {

struct TextMetrics { double width = 0.0; double height = 0.0; };

// ASCII 0x20..0x7E 内置字形，其余返回 false。
[[nodiscard]] TERMCHART_API bool has_glyph(char32_t codepoint) noexcept;

// 文本像素尺寸：width = 列数 × cell_w（列数来自 util 的显示宽度，CJK=2 列），height = cell_h。
[[nodiscard]] TERMCHART_API TextMetrics measure_text(std::string_view utf8_text,
                                                     double cell_w, double cell_h) noexcept;

// 在 (at.x, at.y) 为**左上角**画一行 UTF-8 文本，返回实际占用宽度（= measure 的 width）。
// 缺字码点画"虚线占位框"（占 display_width 格），保证中文可见、可度量。
[[nodiscard]] TERMCHART_API double draw_text(image::Bitmap& bitmap, std::string_view utf8_text,
                                             Point at, Rgb color,
                                             double cell_w, double cell_h) noexcept;
}  // namespace termchart::gui
#endif
