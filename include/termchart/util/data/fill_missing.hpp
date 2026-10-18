// util/data/fill_missing.hpp —— 缺失值处理（NaN 视为缺失）
//
// NaN 判定：std::isnan(value) —— IEEE 754 静默 NaN / 信号 NaN 都算缺失。
// 三种策略：
//   ForwardFill   ：沿用前一个非缺失值（适合时间序列）
//   LinearInterp  ：相邻非缺失值线性插值（端点外用前/后端值延拓）
//   Drop          ：返回新 vector 仅含非缺失值
//
// 输出：就地把目标 vector 改写（Fill 类）或返回新 vector（Drop）。
#ifndef TERMCHART_UTIL_DATA_FILL_MISSING_HPP
#define TERMCHART_UTIL_DATA_FILL_MISSING_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/export.hpp"

namespace termchart::util::data {

enum class FillStrategy : std::uint8_t {
    ForwardFill,
    LinearInterp,
    Drop,
};

[[nodiscard]] TERMCHART_API const char* fill_strategy_name(FillStrategy s) noexcept;

// 改写入参；全 NaN 全部保持 NaN（ForwardFill/LinearInterp 都这样）。
[[nodiscard]] TERMCHART_API std::size_t
fill_missing(std::vector<double>& values, FillStrategy strategy) noexcept;

// Drop：返回新 vector（仅非缺失值）。
[[nodiscard]] TERMCHART_API std::vector<double>
drop_missing(const std::vector<double>& values);

// 计数：返回 NaN 个数。
[[nodiscard]] TERMCHART_API std::size_t
count_missing(const std::vector<double>& values) noexcept;

}  // namespace termchart::util::data

#endif  // TERMCHART_UTIL_DATA_FILL_MISSING_HPP
