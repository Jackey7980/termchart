// util/data/aggregate.hpp —— 聚合（group by + reducer）
//
// 设计目标：pandas DataFrame.groupby().agg() 的"够用版"，零依赖。
// 数据模型：`DataView` 是对外部 std::vector<double> 的非持有视图（指针+长度），
// 调用方负责生命周期；多列对齐靠 `Columns` —— 一组同长度 DataView，
// "分组键"由第一列决定（或显式指定 key column）。
//
// 用途：等宽直方图、按类目求和、按时间桶聚合、箱线图四分位数（min/25/50/75/max）。
//
// 约束：单线程；不抛异常；空输入返回空结果。
#ifndef TERMCHART_DATA_AGGREGATE_HPP
#define TERMCHART_DATA_AGGREGATE_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "termchart/export.hpp"

namespace termchart::data {

struct DataView {
    const double* data = nullptr;
    std::size_t   size = 0;
};

enum class Reducer : std::uint8_t {
    Sum, Mean, Min, Max, Count, First, Last, Median, Stddev,
};

[[nodiscard]] TERMCHART_API const char* reducer_name(Reducer r) noexcept;

struct Group {
    double                key = 0.0;       // 分组键（NaN 不参与聚合）
    // 每列一个非持有视图。**所有权在 owned**：values_per_col[i] 指向
    // owned[i] 的堆缓冲——vector<vector> 移动只转 24 字节描述符，内层
    // 堆数据不动，指针跨 Group 移动/out push_back 始终有效。
    // （2026-10-22 修复：此前在循环内声明 flat 后存 DataView，flat 析构
    // 即成悬垂指针——UB，小数据下"碰巧不崩"正是其阴险处。）
    std::vector<std::vector<double>> owned;
    std::vector<DataView>            values_per_col;
};

// 基础聚合：单列按 key 分组，对 value 列施加 reducer。
// `key_view` 与 `value_view` 必须同长度；NaN key 跳过。
[[nodiscard]] TERMCHART_API std::vector<Group>
group_reduce(DataView key_view, std::vector<DataView> value_views, Reducer reducer);

// 多 reducer：同组输出多个聚合结果。
struct MultiReduceResult {
    std::vector<Group>   groups;   // 同一组只出现一次（按 key 升序）
    std::vector<Reducer> reducers; // 与 groups[i].values_per_col 对应
};
[[nodiscard]] TERMCHART_API MultiReduceResult
group_reduce_multi(DataView key_view,
                   std::vector<DataView> value_views,
                   std::vector<Reducer> reducers);

// 直方图：等宽 bin，bin 数由调用方给，返回每桶计数。
struct HistogramBin {
    double lo = 0.0;     // 包含
    double hi = 0.0;     // 不包含（最后一桶包含 hi）
    std::size_t count = 0;
};
[[nodiscard]] TERMCHART_API std::vector<HistogramBin>
histogram(DataView view, std::size_t num_bins);

// 四分位数（min / Q1 / median / Q3 / max）。NaN 跳过；偶数样本中位数为两均值。
struct Quartiles {
    double minimum = 0.0;
    double q1 = 0.0;
    double median = 0.0;
    double q3 = 0.0;
    double maximum = 0.0;
};
[[nodiscard]] TERMCHART_API Quartiles quartiles(DataView view);

}  // namespace termchart::data

#endif  // TERMCHART_DATA_AGGREGATE_HPP
