// quantize.hpp —— 真彩 → 256 → 16 降级
//
// 颜色管线的一环（docs/design/color_pipeline.md）：
//   真彩 Rgb →（终端能力）→ Ansi256 / Ansi16 / None。
// 能力本身由 term/caps.hpp 给出，本文件只做"退化算法"，不知道终端是否存在。
#ifndef TERMCHART_COLOR_QUANTIZE_HPP
#define TERMCHART_COLOR_QUANTIZE_HPP

#include <cstdint>

#include "termchart/core/color_depth.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::color {

// xterm-256 立方格 / 灰阶映射。索引可逆（见 from_index_256）。
TERMCHART_API std::uint8_t to_index_256(Rgb color) noexcept;

// 最近邻映射到标准 16 色（含高亮档 8..15）。
TERMCHART_API std::uint8_t to_index_16(Rgb color) noexcept;

TERMCHART_API Rgb from_index_256(std::uint8_t index) noexcept;
TERMCHART_API Rgb from_index_16(std::uint8_t index) noexcept;

// 语义色 → 在给定能力下能表达的色。None 档返回"未指定"，让终端沿用默认色。
TERMCHART_API Color degrade(Color color, ColorDepth depth) noexcept;
TERMCHART_API Style degrade(Style style, ColorDepth depth) noexcept;

}  // namespace termchart::color

#endif  // TERMCHART_COLOR_QUANTIZE_HPP
