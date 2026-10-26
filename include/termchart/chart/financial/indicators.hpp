// indicators.hpp —— 金融技术指标（纯函数，与图表绘制解耦）
//
// 输入约定：收盘价序列按时间升序；所有函数输出长度与输入一致，头部不足窗口处
// 及非有限输入处填 NaN。本模块只做数值计算，不画任何东西——叠加到 candlestick
// 等图上的渲染由调用方负责。计算依赖 math::mean / math::stddev，未引入新依赖。
#ifndef TERMCHART_CHART_FINANCIAL_INDICATORS_HPP
#define TERMCHART_CHART_FINANCIAL_INDICATORS_HPP

#include <span>
#include <vector>

#include "termchart/export.hpp"

namespace termchart::chart::financial {

// 简单移动平均（SMA）。window >= 1；前 window-1 个不足窗口的位置为 NaN。
[[nodiscard]] TERMCHART_API std::vector<double> sma(std::span<const double> values,
                                                    std::size_t window);

// 指数移动平均（EMA）。alpha = 2/(window+1)（pandas ewm(span=window).mean() 口径）；
// 首值 = values[0]；window >= 1。EMA 不依赖窗口内历史，故全程有值（无前导 NaN）。
[[nodiscard]] TERMCHART_API std::vector<double> ema(std::span<const double> values,
                                                    std::size_t window);

struct BollingerBands {
    std::vector<double> mid;    // 中轨 = SMA(window)
    std::vector<double> upper;  // 上轨 = mid + k * 滚动标准差(ddof=0)
    std::vector<double> lower;  // 下轨 = mid - k * 滚动标准差(ddof=0)
};

// 布林带（BOLL）。默认 window=20、k=2；前 window-1 处 mid/upper/lower 为 NaN。
[[nodiscard]] TERMCHART_API BollingerBands bollinger(std::span<const double> values,
                                                    std::size_t window = 20, double k = 2.0);

struct Macd {
    std::vector<double> macd;    // 快线 - 慢线
    std::vector<double> signal;  // macd 的 EMA(signal_window)
    std::vector<double> hist;    // macd - signal（柱）
};

// MACD。默认 fast=12、slow=26、signal=9；macd 与 signal 均全程有值
// （EMA 从首元素起即定义，signal[0] == macd[0]），hist = macd - signal。
[[nodiscard]] TERMCHART_API Macd macd(std::span<const double> values,
                                      std::size_t fast = 12, std::size_t slow = 26,
                                      std::size_t signal = 9);

}  // namespace termchart::chart::financial

#endif  // TERMCHART_CHART_FINANCIAL_INDICATORS_HPP
