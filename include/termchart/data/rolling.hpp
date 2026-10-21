// data/rolling.hpp —— 滚动窗口聚合
//
// 滚动统计（pandas Series.rolling(window).mean() 的最小集）。
// 中心约定：
//   * 窗口长度 = window，**右对齐**：第 i 个输出由 [i-window+1, i] 决定；
//     前 i < window-1 个输出按 partial = true 处理（NaN 或 min_periods 触发）。
//   * min_periods：窗口内最少非缺失值，少于则输出 NaN。
//   * 支持：mean / sum / stddev / min / max / median。
//   * 不抛异常；空 vector 返回空 vector。
//
// 另提供 EMA（指数移动平均）：O(N) 单遍递推，alpha = 2/(window+1)。
// rolling() 本身为 O(N·window)（窗口内每步重算）；纯 SMA 的 O(N) 滑动求和
// 作为已知优化点记入 backlog，刻意不在本文件展开。
#ifndef TERMCHART_DATA_ROLLING_HPP
#define TERMCHART_DATA_ROLLING_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/export.hpp"

namespace termchart::data {

enum class RollingStat : std::uint8_t {
    Mean, Sum, Stddev, Min, Max, Median,
};

[[nodiscard]] TERMCHART_API const char* rolling_stat_name(RollingStat s) noexcept;

struct RollingOptions {
    std::size_t window = 3;
    std::size_t min_periods = 0;       // 0 = 与 window 同；NaN 跳过
    bool        right_aligned = true;  // false = 中心（左右各半）
};

[[nodiscard]] TERMCHART_API std::vector<double>
rolling(const std::vector<double>& values, RollingStat stat,
        const RollingOptions& options = {}) noexcept;

// 指数移动平均（EMA）。alpha = 2/(window+1)（pandas ewm(span=window).mean() 口径）。
//   * O(N) 单遍递推；首个非 NaN 样本作种子，其输出 = 种子本身；
//   * NaN 输入透传 NaN，但**保持**已有 EMA 状态（后续非 NaN 继续递推）；
//   * window == 0 或空输入返回空 vector。
[[nodiscard]] TERMCHART_API std::vector<double>
ema(const std::vector<double>& values, std::size_t window) noexcept;

}  // namespace termchart::data

#endif  // TERMCHART_DATA_ROLLING_HPP
