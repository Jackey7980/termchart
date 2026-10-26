// drawdown.cpp —— 回撤图实现：水下曲线 + 0 基线填充
#include "termchart/chart/financial/drawdown.hpp"

#include <algorithm>
#include <cmath>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

#include "termchart/chart/scene_sink.hpp"

namespace termchart::chart {

void DrawdownChart::set_prices(std::vector<double> prices, Color color) {
    color_ = color;
    dd_.clear();
    dd_.reserve(prices.size());

    bool has_peak = false;
    double peak = 0.0;
    for (const double price : prices) {
        if (!std::isfinite(price)) {
            continue;  // 非有限价格剔除（与头文件口径一致）
        }
        if (!has_peak) {
            has_peak = true;
            peak = price;
            dd_.push_back(0.0);
            continue;
        }
        peak = std::max(peak, price);
        // peak == 0 时无法定义比值：该点记 0（无回撤），不让 -inf 混进序列。
        dd_.push_back(peak == 0.0 ? 0.0 : price / peak - 1.0);
    }
}

double DrawdownChart::max_drawdown() const noexcept {
    if (dd_.empty()) {
        return 0.0;
    }
    return *std::min_element(dd_.begin(), dd_.end());  // 恒 ≤ 0，取最小即最深
}

void DrawdownChart::draw(DrawContext ctx) {
    if (dd_.empty()) {
        return;
    }
    const double worst = max_drawdown();
    // y 域 [worst, 0]；无回撤时给一格的"水位"避免退化区间。
    const double ymin = worst < 0.0 ? worst * 1.05 : -1.0;
    const double xmin = -0.5;
    const double xmax = static_cast<double>(dd_.size()) - 0.5;

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, xmin, xmax, ymin, 0.0, false, true);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "t", ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "drawdown", ctx.axis_style);
    }

    const Style line_style = Style{}.set_fg(color_);
    const double y_zero = area_.y_scale().forward(0.0);
    const Rect inner = area_.inner_rect();

    for (std::size_t i = 0; i < dd_.size(); ++i) {
        const double x = area_.x_scale().forward(static_cast<double>(i));
        const double y = area_.y_scale().forward(dd_[i]);
        if (options_.fill && dd_[i] < 0.0) {
            // 水下填充：点与 0 基线之间铺 '░'（fill_rect 接受任意方向的矩形）。
            ctx.canvas.fill_rect(Rect{x, std::min(y, y_zero), 1.0, std::fabs(y_zero - y)},
                                 U'░', Style{}.set_fg(color_));
        }
        ctx.canvas.put(Point{x, y}, kLineChar, line_style);
    }
    // 0 基线（水下图的地平线）。
    if (y_zero >= inner.y && y_zero <= inner.bottom()) {
        ctx.canvas.draw_line(Point{inner.x, y_zero}, Point{inner.right(), y_zero}, U'─',
                             Style{}.set_fg(default_color(DefaultColor::Muted)));
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

bool DrawdownChart::to_scene(scene::Scene& out, std::uint32_t width,
                              std::uint32_t height) const {
    if (dd_.empty()) {
        return true;
    }
    const double worst = max_drawdown();
    // y 域 [worst, 0]；无回撤时给一格的"水位"避免退化区间。
    const double ymin = worst < 0.0 ? worst * 1.05 : -1.0;
    const double xmin = -0.5;
    const double xmax = static_cast<double>(dd_.size()) - 0.5;

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(width, height, m, xmin, xmax, ymin, 0.0, false, true);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    SceneSink sink(out);
    layout::draw_grid(sink, area_, xticks, yticks,
                      Style{}.set_fg(default_color(DefaultColor::Muted)));
    layout::draw_axis_x(sink, area_, xticks, "t", Style{});
    layout::draw_axis_y(sink, area_, yticks, "drawdown", Style{});

    const Style line_style = Style{}.set_fg(color_);
    const double y_zero = area_.y_scale().forward(0.0);
    const Rect inner = area_.inner_rect();

    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);

    for (std::size_t i = 0; i < dd_.size(); ++i) {
        const double x = area_.x_scale().forward(static_cast<double>(i));
        const double y = area_.y_scale().forward(dd_[i]);
        if (options_.fill && dd_[i] < 0.0) {
            // 水下填充：点与 0 基线之间铺 '░'（矢量 FillOp 还原为填充色）。
            b.fill(color_);
            b.rect(Rect{x, std::min(y, y_zero), 1.0, std::fabs(y_zero - y)});
        }
        sink.put(Point{x, y}, kLineChar, line_style);
    }
    // 0 基线（水下图的地平线）。
    if (y_zero >= inner.y && y_zero <= inner.bottom()) {
        b.stroke(default_color(DefaultColor::Muted));
        b.move_to(Point{inner.x, y_zero});
        b.line_to(Point{inner.right(), y_zero});
    }

    if (!title_.empty()) {
        layout::draw_title(sink, title_, inner);
    }
    b.end_group(0);
    return true;
}

}  // namespace termchart::chart
