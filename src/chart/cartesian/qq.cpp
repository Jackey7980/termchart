// qq.cpp —— QQ 图实现（正态分位数用 Acklam 逆逼近，绘制参考线 + 样本点）
#include "termchart/chart/cartesian/qq.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "termchart/canvas/canvas.hpp"
#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

// Acklam 正态分布分位数逆逼近（无 refinement 步，绘图用途误差足够小）。
// 分子/分母拆成局部变量，避免深层嵌套三元表达式里的括号层级错误。
double normal_inv(double p) noexcept {
    const double p_low = 0.02425;
    if (p < p_low) {
        const double q = std::sqrt(-2.0 * std::log(p));
        const double num = (((((-7.784894002430293e-03 * q - 3.223964580411365e-01) * q -
                               2.400758277161838e+00) * q - 2.549732539343734e+00) * q +
                             4.374664141464968e+00) * q + 2.938163982698783e+00);
        const double den = ((((7.784695709041462e-03 * q + 3.224671290700398e-01) * q +
                              2.445134137142996e+00) * q + 3.754408661907416e+00) * q + 1.0);
        return num / den;
    }
    if (p <= 1.0 - p_low) {
        const double q = p - 0.5;
        const double r = q * q;
        const double num = (((((-3.969683028665376e+01 * r + 2.209460984245205e+02) * r -
                               2.759285104469687e+02) * r + 1.383577518672690e+02) * r -
                             3.066479806614716e+01) * r + 2.506628277459239e+00) * q;
        const double den = (((((-5.447609879822406e+01 * r + 1.615858368580409e+02) * r -
                               1.556989798598866e+02) * r + 6.680131188771972e+01) * r -
                             1.328068155288572e+01) * r + 1.0);
        return num / den;
    }
    const double q = std::sqrt(-2.0 * std::log(1.0 - p));
    const double num = -((((-7.784894002430293e-03 * q - 3.223964580411365e-01) * q -
                           2.400758277161838e+00) * q - 2.549732539343734e+00) * q +
                         4.374664141464968e+00) * q + 2.938163982698783e+00;
    const double den = ((((7.784695709041462e-03 * q + 3.224671290700398e-01) * q +
                          2.445134137142996e+00) * q + 3.754408661907416e+00) * q + 1.0);
    return num / den;
}

}  // namespace

void QQChart::set_sample(std::vector<double> sample) { sample_ = std::move(sample); }

void QQChart::draw(DrawContext ctx) {
    std::vector<double> s;
    s.reserve(sample_.size());
    for (const double v : sample_) {
        if (std::isfinite(v)) {
            s.push_back(v);
        }
    }
    if (s.empty()) {
        return;
    }
    std::sort(s.begin(), s.end());

    // 理论分位数与样本分位数逐点配对：(normal_inv(p_i), s_i)。
    const auto n = static_cast<double>(s.size());
    std::vector<double> theo(s.size(), 0.0);
    double mean = 0.0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const double p = (static_cast<double>(i) + 0.5) / n;
        theo[i] = normal_inv(p);
        mean += s[i];
    }
    mean /= n;
    double var = 0.0;
    for (const double v : s) {
        var += (v - mean) * (v - mean);
    }
    const double sd = std::sqrt(var / n);

    double x0 = theo.front();
    double x1 = theo.back();
    if (!(x1 > x0)) {
        x0 = -1.0;  // 单点样本：给对称的定义域
        x1 = 1.0;
    }
    double ymin = s.front();
    double ymax = s.back();
    if (!(ymax > ymin)) {
        ymin -= 0.5;
        ymax += 0.5;
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, x0, x1, ymin, ymax, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(Color::ansi16(8))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "theoretical", ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "sample", ctx.axis_style);
    }

    if (options_.show_line) {
        // 参考线 y = μ + σ·x：先画，点盖在线上。
        const Style st = Style{}.set_fg(Color::ansi16(8));
        ctx.canvas.draw_line(area_.cell_of(x0, mean + sd * x0),
                             area_.cell_of(x1, mean + sd * x1), kLineChar, st);
    }
    const Style pt = Style{}.set_fg(options_.point_color);
    for (std::size_t i = 0; i < s.size(); ++i) {
        ctx.canvas.put(area_.cell_of(theo[i], s[i]), kMarkerChar, pt);
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, area_.inner_rect());
    }
}

}  // namespace termchart::chart
