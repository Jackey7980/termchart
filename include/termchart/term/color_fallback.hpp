// color_fallback.hpp —— 颜色降级（TerminalCaps → 实际可用的颜色深度）
//
// 降级**算法只有一份**，在 color/quantize（L1）：真彩 → 256 色 → 16 色 → 无色的
// 量化与最邻近匹配都在那里。本文件是 term 层的桥接：从 TerminalCaps 取出实际可用
// 的 ColorDepth，再转发给 quantize::degrade，避免"每个调用点自己判断一遍"。
#ifndef TERMCHART_TERM_COLOR_FALLBACK_HPP
#define TERMCHART_TERM_COLOR_FALLBACK_HPP

#include "termchart/core/color_depth.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/term/caps.hpp"

namespace termchart::term {

// 终端真正支持的深度（None 表示只能用默认前/背景色）。
[[nodiscard]] TERMCHART_API ColorDepth depth_of(const TerminalCaps& caps) noexcept;

// 把颜色压到该深度；已是可表示的颜色则原样返回。
[[nodiscard]] TERMCHART_API Color apply_fallback(const Color& color,
                                                  const TerminalCaps& caps) noexcept;

// 前/背景色降级，属性位（加粗/下划线等）保持不变。
[[nodiscard]] TERMCHART_API Style apply_fallback(const Style& style,
                                                  const TerminalCaps& caps) noexcept;

}  // namespace termchart::term

#endif  // TERMCHART_TERM_COLOR_FALLBACK_HPP
