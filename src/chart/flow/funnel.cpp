// funnel.cpp —— 漏斗图实现：居中横条递缩
#include "termchart/chart/flow/funnel.hpp"

#include <algorithm>

#include "termchart/chart/hierarchical/node.hpp"
#include "termchart/layout/layout.hpp"
#include "termchart/layout/title.hpp"

namespace termchart::chart {

void FunnelChart::set_stages(std::vector<FunnelStage> stages) {
    stages_ = std::move(stages);
}

void FunnelChart::draw(DrawContext ctx) {
    if (stages_.empty()) {
        return;
    }
    canvas::Canvas& canvas = ctx.canvas;
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{m.left, m.top,
                     static_cast<double>(canvas.width()) - m.left - m.right,
                     static_cast<double>(canvas.height()) - m.top - m.bottom};
    if (inner.w < 4.0 || inner.h < 1.0) {
        return;
    }
    double vmax = 0.0;
    for (const FunnelStage& stage : stages_) {
        vmax = std::max(vmax, stage.value);
    }
    if (vmax <= 0.0) {
        return;
    }

    // 行数多于画布时截断（canvas 裁剪兜底，这里显式限制避免除 0）。
    const std::size_t rows = std::min(stages_.size(),
                                      static_cast<std::size_t>(inner.h));
    for (std::size_t i = 0; i < rows; ++i) {
        const FunnelStage& stage = stages_[i];
        const Color color =
            stage.color.is_none() ? hier::hier_palette(i) : stage.color;
        const Style st = Style{}.set_fg(color);
        const double row_y = inner.y + static_cast<double>(i);
        const double bar_w = inner.w * (stage.value / vmax);
        const double bar_x = inner.x + (inner.w - bar_w) / 2.0;
        canvas.fill_rect(Rect{bar_x, row_y, bar_w, 1.0}, kBarChar, st);

        std::string label = stage.name;
        if (options.show_values) {
            label += ' ';
            label += layout::format_tick(stage.value);
        }
        canvas.draw_text(Point{0.0, row_y}, label, st);
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(canvas, title_, inner);
    }
}

}  // namespace termchart::chart
