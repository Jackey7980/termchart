// font.hpp —— GUI 文本的唯一出口（对调用方透明的混合字体后端）
//
// 渲染后端分工（接口不变，底层实现见 font.cpp）：
//   * 全部码点（含 ASCII 0x20..0x7E）：优先用系统 TTF 光栅化——按平台候选
//     清单加载一族字体（拉丁在前、CJK 在后），每个码点取首个含其轮廓的字体，
//     矢量 4×4 超采样 → 清晰且带字距，不再用内置 5×7 点阵硬撑；
//   * 内置 5×7 点阵 + 虚线占位框：TTF 整体不可用（无系统字体 / 解析失败 /
//     某码点无轮廓）时的 fail-safe 兜底——不抛异常、不崩溃、不留白。
//   系统字体发现：优先用运行时 set_font_file() 指定的字体文件；其次环境变量
//   TERMCHART_GUI_FONT；否则按平台候选清单（Windows 的 Segoe UI/Arial/Consolas…
//   与雅黑/黑体；Linux 的 DejaVu/Liberation 与 Noto CJK/WQY；macOS 的
//   Helvetica/PingFang…）逐个解析。
//   等宽语义不变：列数由 util 的显示宽度决定（CJK 占 2 列），每格 advance
//   恒等于 cell_w；字体只决定格内像素，不参与排版步进（无 shaping/hinting，
//   不存在字距把布局挤歪的问题）。
//
// 字形是 5×7 列位点阵（数据与位序约定见 font.cpp 的 kFont5x7），渲染时映射进
// cell_w×cell_h 的整数格：字形画在前 5/6 格宽、前 7/8 格高（末列/末行留作
// 字距），保证 advance == cell_w 整数对齐。TTF 也没有覆盖到的码点画"虚线
// 占位框"而不是留白——中文标签掉成空白比掉成虚框更难排查，可见且可度量是底线。
#ifndef TERMCHART_GUI_FONT_HPP
#define TERMCHART_GUI_FONT_HPP
#include <string_view>
#include <string>
#include "termchart/core/point.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/export.hpp"
#include "termchart/image/bitmap.hpp"
namespace termchart::gui {

struct TextMetrics { double width = 0.0; double height = 0.0; };

// 字体后端多态基类：gui/font 的唯一文本出口经它分派到具体字形来源。
// 三个派生实现（5×7 点阵 / 系统 TTF / 二者组合的降级链）是
// src/gui/font.cpp 的实现细节，不进公开头——加后端不改公开 API。
//
// 三条虚函数的契约：
//   * has_glyph：本后端是否**可能**覆盖该码点（组合链的委派依据）。
//     允许保守声明（宁可说覆盖、画时再判），因为组合链按 draw_glyph
//     的返回值决定胜出，不依赖 has_glyph 的精确性；
//   * measure_width：单码点像素宽度。等宽网格契约——列数由
//     util::codepoint_width 决定（CJK=2 列），advance = 列数 × cell_w。
//     后端不得自行改变步进，否则排版会因字体环境不同而漂移；
//   * draw_glyph：画一个码点（占 columns 列，at 为格盒左上角）。返回
//     true = 画出了内容；false = 本后端不覆盖，组合链继续问下一个，
//     全不覆盖由 draw_text 画虚线占位框。
class TERMCHART_API FontBackend {
public:
    // 类内只声明、font.cpp 内定义（key function 惯用法）：vtable/typeinfo
    // 只在这一个 TU 发强符号，匿名 namespace 的派生类各自只发弱表。
    virtual ~FontBackend();
    [[nodiscard]] virtual bool has_glyph(char32_t codepoint) const noexcept = 0;
    [[nodiscard]] virtual double measure_width(char32_t codepoint, double cell_w,
                                               double cell_h) const noexcept = 0;
    [[nodiscard]] virtual bool draw_glyph(image::Bitmap& bitmap, char32_t codepoint, Point at,
                                          const Rgb& color, int columns, double cell_w,
                                          double cell_h) const noexcept = 0;
};

// ASCII 0x20..0x7E 内置字形覆盖查询（**契约不变**：只回答内置点阵表，
// 其余返回 false；TTF 命中不经过此查询，TTF 能力走 draw_text 组合链）。
[[nodiscard]] TERMCHART_API bool has_glyph(char32_t codepoint) noexcept;

// 文本像素尺寸：width = 列数 × cell_w（列数来自 util 的显示宽度，CJK=2 列），height = cell_h。
[[nodiscard]] TERMCHART_API TextMetrics measure_text(std::string_view utf8_text,
                                                     double cell_w, double cell_h) noexcept;

// 在 (at.x, at.y) 为**左上角**画一行 UTF-8 文本，返回实际占用宽度（= measure 的 width）。
// 缺字码点画"虚线占位框"（占 display_width 格），保证中文可见、可度量。
[[nodiscard]] TERMCHART_API double draw_text(image::Bitmap& bitmap, std::string_view utf8_text,
                                             Point at, Rgb color,
                                             double cell_w, double cell_h) noexcept;

// 运行时指定 GUI 文本使用的 TTF 字体文件路径：优先于环境变量 TERMCHART_GUI_FONT
// 与系统字体候选清单。须在首次绘制前调用，也可运行中随时切换（下次绘制即生效）。
// 传空串 = 清除覆盖，回落到环境变量 / 系统字体。
TERMCHART_API void set_font_file(std::string path);
TERMCHART_API void clear_font_file() noexcept;
}  // namespace termchart::gui
#endif
