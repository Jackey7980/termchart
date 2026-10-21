// util/data/resample.hpp —— 重采样（按规则桶聚合）
//
// 用途：把不规则间隔的时间序列按"每 N 个一桶"或"按显式桶边界"降采样，
// 输出每桶的聚合值（mean / sum / min / max / first / last）。
//
// 与 `aggregate::histogram` 的差异：resample 不要求等宽，
// 桶边界由 `BinEdges` 显式给出（等宽也只是其中一种生成方式）。
#ifndef TERMCHART_DATA_RESAMPLE_HPP
#define TERMCHART_DATA_RESAMPLE_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/export.hpp"
#include "termchart/data/aggregate.hpp"

namespace termchart::data {

// 等宽桶：lo..hi 切成 n 桶（最后一桶包含 hi）。
[[nodiscard]] TERMCHART_API std::vector<double>
linspace_edges(double lo, double hi, std::size_t n);

// 重采样：把 values 按 edges 切桶，每桶施加 reducer。
// edges 必须升序且至少 2 个元素；values 越界的不进任何桶。
struct ResampleBin {
    double      lo = 0.0;       // 包含
    double      hi = 0.0;       // 不包含（最后一桶包含 hi）
    double      value = 0.0;    // 该桶聚合结果（NaN 表示空桶）
    std::size_t count = 0;      // 该桶样本数
};
[[nodiscard]] TERMCHART_API std::vector<ResampleBin>
resample(const std::vector<double>& values, const std::vector<double>& edges,
         Reducer reducer);

}  // namespace termchart::util::data

#endif  // TERMCHART_DATA_RESAMPLE_HPP
