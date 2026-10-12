// depth.hpp —— 深度图（订单簿买卖两侧阶梯填充）
//
// 字符化策略：买盘（bid）与卖盘（ask）各自按 (price, qty) 画阶梯，
// 阶梯线下方到量基线之间填 ░，两侧同框、以中间价自然分界。
#ifndef TERMCHART_CHART_FINANCIAL_DEPTH_HPP
#define TERMCHART_CHART_FINANCIAL_DEPTH_HPP

#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"

namespace termchart::chart {

struct DepthOptions {
    Color bid = Color::ansi16(2);  // 买盘（绿）
    Color ask = Color::ansi16(1);  // 卖盘（红）
};

class DepthChart final : public Chart {
public:
    void set_book(std::vector<double> bid_price, std::vector<double> bid_qty,
                  std::vector<double> ask_price, std::vector<double> ask_qty);

    void draw(DrawContext ctx) override;

    DepthOptions options;

private:
    std::vector<double> bid_price_;
    std::vector<double> bid_qty_;
    std::vector<double> ask_price_;
    std::vector<double> ask_qty_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_FINANCIAL_DEPTH_HPP
