// hbox.hpp —— 水平盒子布局（子控件自左向右排列，主轴 = x）
#ifndef TERMCHART_WIDGET_HBOX_HPP
#define TERMCHART_WIDGET_HBOX_HPP

#include "termchart/widget/container.hpp"

namespace termchart::widget {

// 水平盒子：子控件按加入顺序自左向右排，主轴为 x，交叉轴为 y。
// 子控件宽度取自其当前 bounds().w；高度按对齐方式决定（Stretch 填满内容高）。
class HBox : public Container {
public:
    explicit HBox(std::string id);

    void set_spacing(double s) noexcept { spacing_ = s; set_dirty(); }
    [[nodiscard]] double spacing() const noexcept { return spacing_; }
    void set_padding(double p) noexcept { padding_ = p; set_dirty(); }
    [[nodiscard]] double padding() const noexcept { return padding_; }
    void set_alignment(Alignment a) noexcept { align_ = a; set_dirty(); }
    [[nodiscard]] Alignment alignment() const noexcept { return align_; }

    void layout() override;

private:
    double spacing_ = 0.0;
    double padding_ = 0.0;
    Alignment align_ = Alignment::Stretch;
};

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_HBOX_HPP
