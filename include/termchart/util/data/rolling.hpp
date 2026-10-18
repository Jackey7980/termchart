// util/data/rolling.hpp —— 滚动窗口聚合
//
// 滚动统计（pandas Series.rolling(window).mean() 的最小集）。
// 中心约定：
//   * 窗口长度 = window，**右对齐**：第 i 个输出由 [i-window+1, i] 决定；
//     前 i < window-1 个输出按 partial = true 处理（NaN 或 min_periods 触发）。
//   * min_periods：窗口内最少非缺失值，少于则输出 NaN。
//   * 支持：mean / sum / stddev / min / max / median。
//   * 不抛异常；空 vector 返回空 vector。
#ifndef TERMCHART_UTIL_DATA_ROLLING_HPP
#define TERMCHART_UTIL_DATA_ROLLING_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/export.hpp"

namespace termchart::util::data {

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

}  // namespace termchart::util::data

#endif  // TERMCHART_UTIL_DATA_ROLLING_HPP
