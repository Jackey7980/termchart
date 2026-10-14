// widget_tree/label.hpp —— 文本标签（最常用的静态控件）
//
// 演示 Widget 的最小可用形态：底色矩形 + 一行点阵文本。绘制走 TkCanvas（即时
// 模式），文本经 BitmapCanvas::draw_text → termchart::gui 5×7 点阵栅格化。
// 垂直居中按字格高 8px 计算；左上留 4px 内边距。命中测试/几何沿用 Node。
#ifndef TERMCHART_GUI_WIDGET_TREE_LABEL_HPP
#define TERMCHART_GUI_WIDGET_TREE_LABEL_HPP

#include <string>

#include "termchart/core/style.hpp"
#include "termchart_gui/backend_tk/tk_canvas.hpp"
#include "termchart_gui/fwd.hpp"
#include "termchart_gui/widget_tree/widget.hpp"

namespace termchart_gui {

class Label final : public Widget {
public:
    Label(std::string id, std::string text, Rgb fg = Rgb{232, 232, 236},
          Rgb bg = Rgb{26, 26, 30})
        : Widget(std::move(id)), text_(std::move(text)), fg_(fg), bg_(bg) {}

    void set_text(std::string t) { text_ = std::move(t); }
    [[nodiscard]] const std::string& text() const noexcept { return text_; }

    void set_fg(Rgb c) noexcept { fg_ = c; }
    void set_bg(Rgb c) noexcept { bg_ = c; }

    void paint(TkCanvas& c) const override {
        const Rect b = global_bounds();
        c.fill_rect(b, bg_);
        if (!text_.empty()) {
            const double ty = b.y + (b.h - BitmapCanvasTextH) * 0.5;
            c.draw_text(b.x + 4.0, ty, text_, fg_);
        }
    }

private:
    static constexpr double BitmapCanvasTextH = 8.0;  // 与 BitmapCanvas 字格高一致
    std::string text_;
    Rgb fg_{232, 232, 236};
    Rgb bg_{26, 26, 30};
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_LABEL_HPP
