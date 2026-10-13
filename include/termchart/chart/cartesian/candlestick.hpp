// candlestick.hpp —— K 线图：OHLC → 实体 + 影线
//
// 终端字符化策略：开盘-收盘区间画成 █ 实体块（涨/跌分色），最高-最低画成
// │ 影线（先画影线再画实体，实体自然覆盖中段）；开盘=收盘（十字星）时实体
// 退化为单行 ─。x 轴是 K 线序号（第 i 根），不是连续数值。
#ifndef TERMCHART_CHART_CARTESIAN_CANDLESTICK_HPP
#define TERMCHART_CHART_CARTESIAN_CANDLESTICK_HPP

#include <string>
#include <utility>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/layout/box.hpp"

namespace termchart::chart {

class CandlestickChart final : public Chart {
public:
    struct Ohlc {
        double open = 0.0;
        double high = 0.0;
        double low = 0.0;
        double close = 0.0;
    };
    struct Options {
        Color up = default_color(DefaultColor::Accent);
        Color down = default_color(DefaultColor::Warn);
        bool show_wicks = true;
    };

    void set_options(Options opts) { opts_ = std::move(opts); }
    void set_data(std::vector<Ohlc> candles) { candles_ = std::move(candles); }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    Options opts_{};
    std::vector<Ohlc> candles_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_CANDLESTICK_HPP
