// vbox.hpp —— 垂直盒子布局（子控件自上而下堆叠，主轴 = y）
#ifndef TERMCHART_WIDGET_VBOX_HPP
#define TERMCHART_WIDGET_VBOX_HPP

#include "termchart/widget/container.hpp"

namespace termchart::widget {

// 垂直盒子：子控件按加入顺序自上而下排，主轴为 y，交叉轴为 x。
// 子控件的高度取自其当前 bounds().h（调用方应先设好期望高度）；宽度按
// 对齐方式决定（Stretch 填满内容宽，其余保留自身宽度）。
class VBox : public Container {
public:
    explicit VBox(std::string id);

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

#endif  // TERMCHART_WIDGET_VBOX_HPP
