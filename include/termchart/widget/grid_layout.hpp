// grid_layout.hpp —— 网格布局（子控件按行列均分单元格）
#ifndef TERMCHART_WIDGET_GRID_LAYOUT_HPP
#define TERMCHART_WIDGET_GRID_LAYOUT_HPP

#include <cstddef>

#include "termchart/widget/container.hpp"

namespace termchart::widget {

// 网格布局：把内容区均分为 columns × rows 的单元格，按加入顺序先行后列填入。
// 每个子控件在其单元格内按对齐方式定位（Stretch 填满单元格）。
class GridLayout : public Container {
public:
    explicit GridLayout(std::string id);

    void set_columns(std::size_t cols) noexcept { columns_ = cols ? cols : 1; set_dirty(); }
    [[nodiscard]] std::size_t columns() const noexcept { return columns_; }
    void set_spacing(double s) noexcept { spacing_ = s; set_dirty(); }
    [[nodiscard]] double spacing() const noexcept { return spacing_; }
    void set_padding(double p) noexcept { padding_ = p; set_dirty(); }
    [[nodiscard]] double padding() const noexcept { return padding_; }
    void set_alignment(Alignment a) noexcept { align_ = a; set_dirty(); }
    [[nodiscard]] Alignment alignment() const noexcept { return align_; }

    void layout() override;

private:
    std::size_t columns_ = 1;
    double spacing_ = 0.0;
    double padding_ = 0.0;
    Alignment align_ = Alignment::Center;
};

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_GRID_LAYOUT_HPP
