// render/style_map.hpp —— core::Style → GUI 样式
//
// GUI 与终端是平行栈，但颜色必须一致：GUI 显示的像素色必须经 color::resolve
// 按目标 ColorDepth 量化（GUI 不能假装能显示真彩），否则同一张图在终端和窗口里
// 颜色分叉。本文件是那道转换的 GUI 侧薄封装。
#ifndef TERMCHART_GUI_RENDER_STYLE_MAP_HPP
#define TERMCHART_GUI_RENDER_STYLE_MAP_HPP

#include "termchart/core/attr.hpp"
#include "termchart/core/color_depth.hpp"
#include "termchart/core/style.hpp"
#include "termchart/color/resolve.hpp"
#include "termchart_gui/fwd.hpp"

namespace termchart_gui {

struct GuiStyle {
    Rgb fg{0, 0,0};
    Rgb bg{0, 0, 0};
    bool bg_transparent = true;  // 未指定背景色：与下层混合，不填默认背景
    bool bold = false;
    bool underline = false;
};

// core::Style → GuiStyle。属性位（加粗/下划线）从原 Style 读取；颜色经量化。
inline GuiStyle gui_style_from(
    const Style& s,
    termchart::ColorDepth depth = termchart::ColorDepth::TrueColor) {
    const termchart::color::ResolveOptions opts{depth};
    const termchart::color::ResolvedStyle r = termchart::color::resolve_style(s, opts);
    GuiStyle g;
    g.fg = r.fg;
    g.bg = r.bg;
    g.bg_transparent = r.bg_transparent;
    g.bold = s.has_attr(termchart::Attr::Bold);
    g.underline = s.has_attr(termchart::Attr::Underline);
    return g;
}

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_RENDER_STYLE_MAP_HPP
