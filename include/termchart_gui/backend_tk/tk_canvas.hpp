// backend_tk/tk_canvas.hpp —— 像素绘制表面抽象
//
// GUI 的原子是"像素矩形 + 真彩"，与终端的字符格/CellBuffer 不同（gui_stack.md：
// 平行栈不碰 cell/SGR）。TkCanvas 是绘制接口；无头实现把它落到一个 image::Bitmap。
#ifndef TERMCHART_GUI_BACKEND_TK_TK_CANVAS_HPP
#define TERMCHART_GUI_BACKEND_TK_TK_CANVAS_HPP

#include <string_view>

#include "termchart_gui/fwd.hpp"

namespace termchart_gui {

class TkCanvas {
public:
    virtual ~TkCanvas() = default;

    virtual void clear(Rgb bg) = 0;
    virtual void fill_rect(const Rect& rect, Rgb color) = 0;
    virtual void stroke_rect(const Rect& rect, Rgb color, double width) = 0;
    virtual void draw_text(double x, double y, std::string_view text, Rgb color) = 0;

    // 把一张位图（左上角对齐 (x,y)）合成进画布：GUI 组合图表的出口——控件用 Scene
    // 拼好图、经 SceneRenderer 光栅化成 Bitmap，再 blit 上来（抗锯齿白拿）。
    // opacity∈[0,1]：整体缩放源 alpha（1=原样）。半透明合成在这里做——Scene IR 刻意
    // 不带 alpha（见 scene/command.hpp），"图层/透明度"是合成层能力，不属于矢量 IR。
    virtual void blit(const Bitmap& src, double x, double y, double opacity = 1.0) = 0;

    // 取当前位图（用于测试断言 / PNG 导出）。
    [[nodiscard]] virtual Bitmap snapshot() const = 0;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_BACKEND_TK_TK_CANVAS_HPP
