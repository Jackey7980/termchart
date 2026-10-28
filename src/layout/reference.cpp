// reference.cpp —— y 轴零线参考线 + 负区底色实现
#include "termchart/layout/reference.hpp"

#include <cmath>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/attr.hpp"
#include "termchart/layout/canvas_sink.hpp"
#include "termchart/layout/sink.hpp"

namespace termchart::layout {
namespace {

// 0 是否落在数据域内。
bool zero_in_domain(const scale::Scale& s) noexcept {
    const double dmin = s.domain_min();
    const double dmax = s.domain_max();
    return dmin <= 0.0 && 0.0 <= dmax;
}

// 负区在像素空间是 [forward(0), forward(domain_min)] 这一带——数据越小越靠下
// （y 向下增长），故 domain_min 对应 inner 底边附近。
double neg_band_bottom(const scale::Scale& s) noexcept { return s.forward(s.domain_min()); }

}  // namespace

void draw_zero_line_y(Sink& sink, const Rect& inner, const scale::Scale& y_scale,
                      const Style& style) {
    if (!zero_in_domain(y_scale)) {
        return;
    }
    const double y0 = y_scale.forward(0.0);
    if (y0 < inner.y - 0.5 || y0 > inner.bottom() + 0.5) {
        return;  // 零线落在绘图区外（域含 0 但被裁剪），不画
    }
    // 零线强调：即便调用方没指定，也强制加粗，使其在网格/普通轴线中突出。
    Style s = style;
    s.set_attr(Attr::Bold);
    sink.line(Point{inner.x, y0}, Point{inner.right(), y0}, LineStyle::Solid, s);
}

void draw_zero_line_y(canvas::Canvas& canvas, const Rect& inner, const scale::Scale& y_scale,
                      const Style& style) {
    CanvasSink sink(canvas);
    draw_zero_line_y(sink, inner, y_scale, style);
}

void draw_zero_line_y(Sink& sink, const PlotArea& area, const scale::Scale& y_scale,
                      const Style& style) {
    draw_zero_line_y(sink, area.inner_rect(), y_scale, style);
}

void draw_zero_line_y(canvas::Canvas& canvas, const PlotArea& area, const scale::Scale& y_scale,
                      const Style& style) {
    draw_zero_line_y(canvas, area.inner_rect(), y_scale, style);
}

void shade_negative_y(canvas::Canvas& canvas, const Rect& inner, const scale::Scale& y_scale,
                      const Style& style) {
    if (!(y_scale.domain_min() < 0.0)) {
        return;  // 域不含负数：不画
    }
    const double y0 = y_scale.forward(0.0);
    if (std::isnan(y0)) {
        return;  // NaN 守卫
    }
    const double top = std::max(y0, inner.y);
    const double bot = std::min(inner.bottom(), neg_band_bottom(y_scale));
    if (bot <= top + 0.5) {
        return;
    }
    // 空格 + 背景样式：只铺底色，不覆盖已有字符的可见字形。
    canvas.fill_rect(Rect{inner.x, top, inner.w, bot - top}, U' ', style);
}

void shade_negative_y(canvas::Canvas& canvas, const PlotArea& area, const scale::Scale& y_scale,
                      const Style& style) {
    shade_negative_y(canvas, area.inner_rect(), y_scale, style);
}

}  // namespace termchart::layout
