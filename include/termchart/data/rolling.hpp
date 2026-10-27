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
//
// 2026-10-22 重写后，rolling() 全体统计量均为 **O(N)** 滑动窗口（不再每步重算整个窗口）：
//   * Mean / Sum  ：滑动和（SlidingSum，add/remove 各 O(1)）
//   * Stddev      ：Welford 加/减（数值稳定，O(N)）
//   * Min / Max   ：单调双端队列（O(N)）
//   * Median      ：有序双 multiset（O(N log W)）
// 此前"纯 SMA 的 O(N) 滑动求和作为已知优化点"的 backlog 描述已过时——本文件现已是 O(N) 家族，
// 旧注释保留仅作历史对照。
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
