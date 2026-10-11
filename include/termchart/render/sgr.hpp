// sgr.hpp —— SGR（颜色 / 属性）序列的**唯一生成点**
//
// 阶段 1 的硬决策（ROADMAP R2）：所有 SGR 只从这里出去。
//   * 终端实时路径（render/renderer.hpp）用它；
//   * snapshot 导出（backend/snapshot/ansi_backend.hpp）也用它 —— 为此登记了
//     唯一收窄边 `backend/snapshot/ansi_backend > render/sgr`（见 cmake/TermchartModules.cmake）。
// 后果：改序列化规则只需改本文件，所有路径同步生效，不会出现"两套 SGR"。
#ifndef TERMCHART_RENDER_SGR_HPP
#define TERMCHART_RENDER_SGR_HPP

#include <string>

#include "termchart/core/color_depth.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::render::sgr {

// 完整样式序列（含从"默认状态"出发的全部参数）。depth == None 时不输出颜色参数。
TERMCHART_API void append_style(std::string& out, const Style& style, ColorDepth depth) noexcept;
TERMCHART_API std::string style_sequence(const Style& style, ColorDepth depth) noexcept;

// 增量：只输出 from → to 变化的那部分。渲染器每帧的输出量取决于它。
// 属性只发"变化的位"；颜色未变则完全不产生字节。
TERMCHART_API void append_style_delta(std::string& out, const Style& from, const Style& to,
                                      ColorDepth depth) noexcept;

// 复位到终端默认：ESC[0m。行尾/帧尾收口用。
TERMCHART_API std::string reset() noexcept;

// 生成"关闭指定属性"的序列（bold→22 等）；供 delta 内部与测试复用。
TERMCHART_API void append_attr_off(std::string& out, AttrMask off_bits) noexcept;
TERMCHART_API void append_attr_on(std::string& out, AttrMask on_bits) noexcept;

// 单个颜色的参数段（不含 ESC[ 与 m）：38;2;r;g;b / 38;5;n / 31 / 39 ...
TERMCHART_API void append_color_params(std::string& out, const Color& color, bool foreground,
                                       ColorDepth depth) noexcept;

}  // namespace termchart::render::sgr

#endif  // TERMCHART_RENDER_SGR_HPP
