// data/ohlc.hpp —— 时间轴重采样 → K 线 OHLC（L4）
//
// 按等宽时间桶把 (timestamp, value) 序列聚合成 K 线四价：open/high/low/close。
// 与 resample.hpp 的分工：那边按**值域**分桶（直方图式），这边按**时间轴**
// 分桶（金融序列式）——K 线图（chart/cartesian/candlestick）的数据入口。
//
// 约定：
//   * timestamps 必须**非降序**（时间序列常态；io/csv 读入天然有序。
//     乱序输入行为未定义——调用方需先自行排序）；
//   * 桶边界**锚定在每个桶的首个样本**（不规则采样下的合理口径，且单遍 O(N)）；
//   * 空桶不产生 bar（顺序合并不感知未来样本，无法回填空桶）；
//   * NaN 值的样本跳过（不参与任何价格）；
//   * bucket_seconds ≤ 0 = 单桶（全量一根，用于"整段统计"）。
//   * 单位为秒（Unix 时间戳口径）；调用方持有分钟/小时刻度自行换算。
#ifndef TERMCHART_DATA_OHLC_HPP
#define TERMCHART_DATA_OHLC_HPP

#include <cstddef>
#include <span>
#include <vector>

#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"

namespace termchart::data {

struct OhlcBar {
    double t0 = 0.0;       // 桶起点（本桶首个样本的时间戳）
    double open = 0.0;     // 本桶首个非 NaN 样本
    double high = 0.0;     // 桶内最大
    double low = 0.0;      // 桶内最小
    double close = 0.0;    // 桶内最后一个非 NaN 样本
    std::size_t count = 0; // 桶内非 NaN 样本数
};

struct OhlcOptions {
    double bucket_seconds = 60.0;  // 桶宽（秒）；≤ 0 = 单桶全量
};

// 聚合为 OHLC 序列。t/v 取较短长度；空输入返回空 vector。
// timestamps 必须**非降序**——违反时 fail-loud 返回 InvalidArgument
// （2026-10-22 加固：无序输入会把 K 线切成一堆错乱桶，静默产出比报错
// 更糟；并发收到的数据请调用方先自行排序）。
[[nodiscard]] TERMCHART_API Result<std::vector<OhlcBar>> ohlc(
    std::span<const double> timestamps, std::span<const double> values,
    const OhlcOptions& options = {}) noexcept;

}  // namespace termchart::data

#endif  // TERMCHART_DATA_OHLC_HPP
