// chart.hpp —— Chart 抽象基类
//
// chart/ 是 stage 3 的顶点：把数据系列 + 比例尺 + layout 拼成可见图表，画进
// canvas::Canvas。**不碰 render 层**（架构禁止边），于是未来 to_document()
// 通道独立存在。绘制上下文见 context.hpp。
#ifndef TERMCHART_CHART_CHART_HPP
#define TERMCHART_CHART_CHART_HPP

#include <optional>
#include <string>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/context.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/scale/auto_domain.hpp"

namespace termchart::chart {

// 折线/柱共享的字符与几何辅助（定义见 chart.cpp）。
inline constexpr char32_t kLineChar = U'\u2022';     // • 连线
inline constexpr char32_t kMarkerChar = U'\u25CF';   // ● 数据点
inline constexpr char32_t kBarChar = U'\u2588';      // █ 柱

// 默认配色的唯一真源在 `chart/series.hpp`（`DefaultColor` / `default_color()`）——
// 放在那里是因为 chart.hpp 自己 include series.hpp，反过来不行。

void data_bounds(const std::vector<Series>& series, double& xmin, double& xmax,
                  double& ymin, double& ymax);
// y_override：显式 Y 域（**仅 Y**，X 仍按 auto_domain 规整）。给"轴域不该取
// 极值"的场景用——离群裁剪（autoviz 的稳健域）、固定域对比图。
// 有值时它是权威值：不再 nice_domain、不因 include_zero 扩域，**include_zero
// 由调用方自己并进域里**（柱状图必须含 0，见 BarChart::set_y_domain）。
// 钳制由比例尺完成（LinearScale 默认 clamp=true），域外的点自动落在轴边缘。
layout::PlotArea make_area(canvas::Canvas& canvas, const layout::Margin& margin,
                           double xmin, double xmax, double ymin, double ymax,
                           bool auto_domain, bool include_zero,
                           std::optional<scale::Domain> y_override = std::nullopt);
void draw_legend_for(canvas::Canvas& canvas, const std::vector<Series>& series,
                     const layout::PlotArea& area, bool show);

class Chart {
public:
    virtual ~Chart();
    virtual void draw(DrawContext ctx) = 0;

    void set_title(std::string title) { title_ = std::move(title); }
    [[nodiscard]] const std::string& title() const noexcept { return title_; }

    void set_margins(double top, double right, double bottom, double left) {
        margin_top_ = top;
        margin_right_ = right;
        margin_bottom_ = bottom;
        margin_left_ = left;
    }
    [[nodiscard]] double margin_top() const noexcept { return margin_top_; }
    [[nodiscard]] double margin_right() const noexcept { return margin_right_; }
    [[nodiscard]] double margin_bottom() const noexcept { return margin_bottom_; }
    [[nodiscard]] double margin_left() const noexcept { return margin_left_; }

protected:
    std::string title_;
    double margin_top_ = 1.0;
    double margin_right_ = 1.0;
    double margin_bottom_ = 3.0;
    double margin_left_ = 8.0;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CHART_HPP
