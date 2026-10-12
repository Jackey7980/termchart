// candlestick.cpp —— K 线图实现（影线先画、实体后画，实体自然覆盖中段）
#include "termchart/chart/cartesian/candlestick.hpp"

#include <algorithm>
#include <cmath>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

inline constexpr char32_t kBodyChar = U'\u2588';   // █ 实体
inline constexpr char32_t kWickChar = U'\u2502';   // │ 影线
inline constexpr char32_t kDojiChar = U'\u2500';   // ─ 十字星

}  // namespace

void CandlestickChart::draw(DrawContext ctx) {
    // 只保留 OHLC 全有限且 low<=high 的 K 线；其余整根剔除。
    std::vector<Ohlc> candles;
    candles.reserve(candles_.size());
    for (const Ohlc& c : candles_) {
        if (std::isfinite(c.open) && std::isfinite(c.high) && std::isfinite(c.low) &&
            std::isfinite(c.close) && c.low <= c.high) {
            candles.push_back(c);
        }
    }
    if (candles.empty()) {
        return;
    }

    double ymin = candles.front().low;
    double ymax = candles.front().high;
    for (const Ohlc& c : candles) {
        ymin = std::min(ymin, c.low);
        ymax = std::max(ymax, c.high);
    }

    const auto n = static_cast<double>(candles.size());
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    // x 是序号域 [-0.5, n-0.5]（第 i 根落在 x=i），不做 nice_domain；
    // y 直接取 [最低, 最高]，让极值贴近绘图区边缘。
    area_ = make_area(ctx.canvas, m, -0.5, n - 0.5, ymin, ymax, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    const Style axis_style = ctx.axis_style;
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(Color::ansi16(8))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "index", axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "price", axis_style);
    }

    const Rect inner = area_.inner_rect();
    // 实体宽：占格宽的 60%，但至少 1 格（否则窄图上 fill_rect 取整后为空）。
    const double bw = std::max(1.0, inner.w / n * 0.6);

    for (std::size_t i = 0; i < candles.size(); ++i) {
        const Ohlc& c = candles[i];
        const double xc = area_.x_scale().forward(static_cast<double>(i));
        const bool rising = c.close >= c.open;
        const Style st = Style{}.set_fg(rising ? opts_.up : opts_.down);

        if (opts_.show_wicks && c.high > c.low) {
            ctx.canvas.draw_line(
                Point{xc, area_.y_scale().forward(c.high)},
                Point{xc, area_.y_scale().forward(c.low)}, kWickChar, st);
        }

        const double y_open = area_.y_scale().forward(c.open);
        const double y_close = area_.y_scale().forward(c.close);
        if (c.open == c.close) {
            // 十字星：单行 ─。
            ctx.canvas.fill_rect(Rect{xc - bw / 2.0, y_open, bw, 1.0}, kDojiChar, st);
        } else {
            const double top = std::min(y_open, y_close);
            const double h = std::max(1.0, std::fabs(y_close - y_open));
            ctx.canvas.fill_rect(Rect{xc - bw / 2.0, top, bw, h}, kBodyChar, st);
        }
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

}  // namespace termchart::chart
