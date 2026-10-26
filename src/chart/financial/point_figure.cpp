// point_figure.cpp —— 点数图实现：格档量化 + 反转开新列
#include "termchart/chart/financial/point_figure.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

#include "termchart/chart/scene_sink.hpp"

namespace termchart::chart {

void PointFigureChart::set_prices(std::vector<double> prices) {
    prices_ = std::move(prices);
    columns_.clear();

    std::vector<double> finite;
    finite.reserve(prices_.size());
    for (double p : prices_) {
        if (std::isfinite(p)) {
            finite.push_back(p);
        }
    }
    if (finite.size() < 2) {
        return;
    }
    const auto [lo_it, hi_it] = std::minmax_element(finite.begin(), finite.end());
    const double range = *hi_it - *lo_it;
    if (!(range > 0.0)) {
        return;
    }
    double box = options.box_size;
    if (!(box > 0.0) || !std::isfinite(box)) {
        box = range / 10.0;
    }
    const int rev = std::max(options.reversal, 1);

    auto box_of = [&](double p) { return static_cast<int>(std::floor(p / box)); };

    // 首列为 X 列，top = bottom = 首价格档。
    PfColumn col{true, box_of(finite.front()), box_of(finite.front())};
    for (std::size_t i = 1; i < finite.size(); ++i) {
        const int b = box_of(finite[i]);
        if (col.up) {
            if (b > col.top) {
                col.top = b;  // 抬顶
            } else if (b <= col.top - rev) {
                // 反转下行：新 O 列顶比原顶低一档。
                columns_.push_back(col);
                col = PfColumn{false, col.top - 1, b};
            }
        } else {
            if (b < col.bottom) {
                col.bottom = b;  // 沉底
            } else if (b >= col.bottom + rev) {
                // 反转上行：新 X 列底比原底高一档。
                columns_.push_back(col);
                col = PfColumn{true, b, col.bottom + 1};
            }
        }
    }
    columns_.push_back(col);
}

void PointFigureChart::draw(DrawContext ctx) {
    if (columns_.empty()) {
        return;
    }
    int lv_min = columns_.front().bottom;
    int lv_max = columns_.front().top;
    for (const PfColumn& c : columns_) {
        lv_min = std::min(lv_min, c.bottom);
        lv_max = std::max(lv_max, c.top);
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, -0.5, static_cast<double>(columns_.size()) - 0.5,
                      static_cast<double>(lv_min), static_cast<double>(lv_max + 1),
                      false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "column", ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "box", ctx.axis_style);
    }

    for (std::size_t i = 0; i < columns_.size(); ++i) {
        const PfColumn& c = columns_[i];
        const Style st = Style{}.set_fg(c.up ? options.up : options.down);
        const double x = area_.x_scale().forward(static_cast<double>(i));
        for (int lv = c.bottom; lv <= c.top; ++lv) {
            // 格档中心落字符：y 轴反转，档 lv 的中心在 lv + 0.5。
            ctx.canvas.put(
                Point{x, area_.y_scale().forward(static_cast<double>(lv) + 0.5)},
                c.up ? U'X' : U'O', st);
        }
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, area_.inner_rect());
    }
}

bool PointFigureChart::to_scene(scene::Scene& out, std::uint32_t width,
                                 std::uint32_t height) const {
    if (columns_.empty()) {
        return true;
    }
    int lv_min = columns_.front().bottom;
    int lv_max = columns_.front().top;
    for (const PfColumn& c : columns_) {
        lv_min = std::min(lv_min, c.bottom);
        lv_max = std::max(lv_max, c.top);
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(width, height, m, -0.5,
                      static_cast<double>(columns_.size()) - 0.5,
                      static_cast<double>(lv_min), static_cast<double>(lv_max + 1),
                      false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    SceneSink sink(out);
    layout::draw_grid(sink, area_, xticks, yticks,
                      Style{}.set_fg(default_color(DefaultColor::Muted)));
    layout::draw_axis_x(sink, area_, xticks, "column", Style{});
    layout::draw_axis_y(sink, area_, yticks, "box", Style{});

    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);

    for (std::size_t i = 0; i < columns_.size(); ++i) {
        const PfColumn& c = columns_[i];
        const Style st = Style{}.set_fg(c.up ? options.up : options.down);
        const double x = area_.x_scale().forward(static_cast<double>(i));
        // 格档中心落字符：y 轴反转，档 lv 的中心在 lv + 0.5（与 draw() 一致）。
        for (int lv = c.bottom; lv <= c.top; ++lv) {
            sink.put(Point{x, area_.y_scale().forward(static_cast<double>(lv) + 0.5)},
                     c.up ? U'X' : U'O', st);
        }
    }

    if (!title_.empty()) {
        layout::draw_title(sink, title_, area_.inner_rect());
    }
    b.end_group(0);
    return true;
}

}  // namespace termchart::chart
