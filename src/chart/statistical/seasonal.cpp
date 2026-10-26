// seasonal.cpp —— 季节分解实现：居中移动平均 + 相位平均 + 三分带绘制
#include "termchart/chart/statistical/seasonal.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

}  // namespace

void SeasonalChart::set_values(std::vector<double> values, Color color) {
    values_ = std::move(values);
    color_ = color;
    decompose();
}

void SeasonalChart::decompose() {
    trend_.clear();
    seasonal_.clear();
    remainder_.clear();
    const std::size_t n = values_.size();
    const auto period =
        static_cast<std::size_t>(std::max(options_.period, 2));
    if (n < 2 * period) {
        return;  // 不足两个周期：分解无意义
    }

    // 居中移动平均：窗口 period，边缘不足半窗为 NaN。
    // 滑动窗口求和 O(n)（原朴素二重循环为 O(n·period)），数值结果完全一致。
    const std::size_t half = period / 2;
    trend_.assign(n, kNaN);
    double acc = 0.0;
    for (std::size_t k = 0; k < period; ++k) {
        acc += values_[k];  // 首个窗口 [0, period-1]，对应 i = half
    }
    for (std::size_t i = half; ; ) {
        trend_[i] = acc / static_cast<double>(period);
        const std::size_t next = i + 1;
        if (next + (period - half) > n) {
            break;  // 下一窗口右端越界，停止
        }
        acc -= values_[i - half];                  // 滑出窗口的左端
        acc += values_[next + (period - half) - 1];  // 滑入窗口的新元素
        i = next;
    }

    // 相位平均去趋势残差 → 季节分量（循环铺满）。
    std::vector<double> phase_sum(period, 0.0);
    std::vector<std::size_t> phase_cnt(period, 0);
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isnan(trend_[i])) {
            phase_sum[i % period] += values_[i] - trend_[i];
            ++phase_cnt[i % period];
        }
    }
    std::vector<double> phase_mean(period, 0.0);
    for (std::size_t j = 0; j < period; ++j) {
        phase_mean[j] =
            phase_cnt[j] > 0
                ? phase_sum[j] / static_cast<double>(phase_cnt[j])
                : 0.0;
    }
    seasonal_.assign(n, kNaN);
    for (std::size_t i = 0; i < n; ++i) {
        seasonal_[i] = phase_mean[i % period];
    }

    remainder_.assign(n, kNaN);
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isnan(trend_[i])) {
            remainder_[i] = values_[i] - trend_[i] - seasonal_[i];
        }
    }
}

void SeasonalChart::draw(DrawContext ctx) {
    if (trend_.empty()) {
        return;
    }
    const std::size_t n = trend_.size();
    double vlo = std::numeric_limits<double>::infinity();
    double vhi = -vlo;
    for (double v : values_) {
        if (std::isfinite(v)) {
            vlo = std::min(vlo, v);
            vhi = std::max(vhi, v);
        }
    }
    if (!std::isfinite(vlo) || !std::isfinite(vhi)) {
        return;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = layout::PlotArea(static_cast<double>(ctx.canvas.width()),
                             static_cast<double>(ctx.canvas.height()), m, 0.0,
                             static_cast<double>(n) - 1.0, vlo, vhi);
    const Rect inner{
        m.left, m.top,
        static_cast<double>(ctx.canvas.width()) - m.left - m.right,
        static_cast<double>(ctx.canvas.height()) - m.top - m.bottom};
    // 三条带 + 两条 1 行间隔；高度不够时宁可不画。
    const double band_h = (inner.h - 2.0) / 3.0;
    if (band_h < 2.0) {
        return;
    }

    const std::vector<double> xticks =
        scale::ticks_for(scale::LinearScale(0.0, static_cast<double>(n) - 1.0,
                                            inner.x, inner.right()));
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, {}, grid_style);
    }

    struct Band {
        const char* label;
        const std::vector<double>* series;
    };
    const Band bands[3] = {
        {"trend", &trend_}, {"season", &seasonal_}, {"remain", &remainder_}};

    for (int b = 0; b < 3; ++b) {
        const std::vector<double>& series = *bands[b].series;
        double lo = std::numeric_limits<double>::infinity();
        double hi = -lo;
        for (double v : series) {
            if (!std::isnan(v)) {
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
        }
        if (!std::isfinite(lo) || !std::isfinite(hi)) {
            continue;
        }
        if (!(hi > lo)) {
            lo -= 0.5;
            hi += 0.5;
        }
        // 带 b 的顶部偏移：0、band_h+1、2(band_h+1)；底部相应收缩。
        const double gap = static_cast<double>(b) * (band_h + 1.0);
        const layout::Margin bm{m.top + gap, m.right,
                                m.bottom + (2.0 * (band_h + 1.0) - gap), m.left};
        const layout::PlotArea band_area(
            static_cast<double>(ctx.canvas.width()),
            static_cast<double>(ctx.canvas.height()), bm, 0.0,
            static_cast<double>(n) - 1.0, lo, hi);
        const Style st = Style{}.set_fg(color_);
        for (std::size_t i = 0; i < n; ++i) {
            if (std::isnan(series[i])) {
                continue;
            }
            ctx.canvas.put(band_area.cell_of(static_cast<double>(i), series[i]),
                           kLineChar, st);
        }
        ctx.canvas.draw_text(Point{0.0, band_area.inner_rect().y},
                             bands[b].label, Style{}.set_fg(default_color(DefaultColor::Series)));
        if (ctx.draw_axes) {
            layout::draw_axis_x(ctx.canvas, band_area, xticks, "", ctx.axis_style);
        }
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

}  // namespace termchart::chart
