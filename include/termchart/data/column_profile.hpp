// data/column_profile.hpp —— 列级画像：AutoViz 阶段二"特征探测"的底座（L4）
//
// 定位（对应 AutoViz 规格书阶段二）：
//   在 DataFrame 之上做**极其克制**的单遍探测——类型判定抽样、基数熔断、
//   空值率分桶。这里只回答"这列像什么、能干什么用"，不做任何绘图决策；
//   路由与否决在 autoviz 模块（L21）。
//
// 与 dsl/csv_sniff.hpp 的 ColumnProfile 分工（刻意两份）：
//   * dsl::ColumnProfile：服务文档表格渲染，只要 filled/empty 计数；
//   * 本文件：服务 AutoViz，要 distinct/基数分档/空值率/极值/单调性。
//   合并会让文档渲染背上统计成本，反过来会让 AutoViz 缺关键词，故并存。
//
// 性能口径：profile_columns 对整表是 O(列数 × 行数) 的单遍（每个 Cell 只
// 碰一次），distinct 探测带熔断（超过 distinct_cap 立即停），类型判定只抽样
// 前 sample_rows 行。不在此处计算方差/相关系数——那些按需、在聚合管线的
// 目的地才算，防止"解析阶段算尽统计量"的性能灾难。
#ifndef TERMCHART_DATA_COLUMN_PROFILE_HPP
#define TERMCHART_DATA_COLUMN_PROFILE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/data/dataframe.hpp"

namespace termchart::data {

// 列级语义类型：Cell 级类型的聚合判定（占比 ≥ kind_ratio 即成立）。
// 判定优先级：Timestamp > Numeric > Boolean > Text——"2024-01-01" 认不出数值
// 归 Text，必须先过时间关；"0/1" 列留 Numeric（可当数值用），不进 Boolean。
enum class ColumnKind : std::uint8_t {
    Unknown,    // 0 行（空表）
    NullOnly,   // 有行但全缺失
    Timestamp,  // 时间格式（ISO-8601 家族，见 looks_like_timestamp）
    Numeric,    // 数值
    Boolean,    // true/false/yes/no（大小写不敏感）
    Text,       // 其余（分类维度或脏文本）
};

// 基数分档：路由看的是"用途资格"，不是裸 distinct 数。
//   Constant → 丢弃；Low → 可分组/可着色；Mid → 仅可 facet 不可着色；
//   High → ID 列，只配去重计数。
enum class CardinalityTier : std::uint8_t {
    Constant,  // distinct ≤ 1
    Low,       // 2..policy.low_max
    Mid,       // low_max+1..policy.mid_max
    High,      // > policy.mid_max
};

// 分档阈值是**策略值不是常量**：交易流水与传感器域对 20/200 的体感不同，
// 调用方可覆盖。默认值对应规格书分档。
struct CardinalityPolicy {
    std::size_t low_max = 20;
    std::size_t mid_max = 200;
};

[[nodiscard]] constexpr CardinalityTier tier_of(std::size_t distinct,
                                                const CardinalityPolicy& policy = {}) noexcept {
    if (distinct <= 1) {
        return CardinalityTier::Constant;
    }
    if (distinct <= policy.low_max) {
        return CardinalityTier::Low;
    }
    if (distinct <= policy.mid_max) {
        return CardinalityTier::Mid;
    }
    return CardinalityTier::High;
}

[[nodiscard]] const char* cardinality_tier_name(CardinalityTier tier) noexcept;

// 单列画像。
struct ColumnProfile {
    std::string name;
    ColumnKind kind = ColumnKind::Unknown;
    std::size_t rows = 0;               // 总行数
    std::size_t non_null = 0;           // 非缺失数
    std::size_t distinct = 0;           // 去重数（按 raw 原文计）
    bool distinct_capped = false;       // true = 探测熔断，distinct/tier 只是**下界**
    double null_rate = 0.0;             // 缺失率（rows=0 时为 0）
    double min = 0.0;                   // Numeric 列有效域；其余为 NaN
    double max = 0.0;
    bool monotonic_nondecreasing = false;  // Numeric 列非空值是否非降序（时序轴校验用）
    CardinalityTier tier = CardinalityTier::Constant;

    [[nodiscard]] bool is_plottable_value() const noexcept {  // 可当 Y 轴/数值用
        return kind == ColumnKind::Numeric && non_null > 0;
    }
    [[nodiscard]] bool is_categorical() const noexcept {  // 可当分组/分类维度用
        return (kind == ColumnKind::Text || kind == ColumnKind::Boolean) && non_null > 0;
    }
};

// 探测选项。默认值对应规格书；全部可调。
struct ProfileOptions {
    CardinalityPolicy cardinality{};      // 基数分档阈值
    std::size_t distinct_cap = 256;       // distinct 熔断阈值（> mid_max 时 capped 必属 High）
    std::size_t sample_rows = 200;        // 类型判定抽样行数（0 = 全量）
    double kind_ratio = 0.8;              // 类型判定的占比阈值
};

// 对整表逐列画像。列名缺省时（headers 短于 columns）自动补 "colN"。
[[nodiscard]] std::vector<ColumnProfile> profile_columns(const DataFrame& frame,
                                                         const ProfileOptions& options = {});

// 分桶空值率：先按 key 列分桶，再在各桶内算 value 列空值率。
// 修正"全局空值率误杀事件关键字段"：duration_min 在 return 桶里几乎不空，
// 全局看却可能被高缺失率一刀切——分桶后才看得见。key 单元格缺失的行无法
// 归属，跳过（不计入任何桶）。
struct BucketNullRate {
    std::string key;
    std::size_t rows = 0;
    std::size_t non_null = 0;
    double null_rate = 0.0;
};

// 结果按 key 字典序排列（确定性输出，便于测试与缓存）。
[[nodiscard]] std::vector<BucketNullRate> null_rate_by_bucket(
    const DataFrame& frame, std::string_view key_column, std::string_view value_column);

// 时间格式识别（ISO-8601 家族的最小实用集）：
//   YYYY-MM-DD
//   YYYY-MM-DD HH:MM[:SS[.fff]]
//   YYYY-MM-DDTHH:MM[:SS[.fff]][Z|±HH:MM|±HHMM]
// 只认形状不认历法（2 月 30 号也过）——路由只需要"这是时间列"；
// 真历法校验在解析成 epoch 的目的地做。列不存在/不匹配均返回 false。
[[nodiscard]] bool looks_like_timestamp(std::string_view text) noexcept;

// 布尔词表：true/false/yes/no（大小写不敏感）。刻意不含 0/1——那是数值。
[[nodiscard]] bool looks_like_boolean(std::string_view text) noexcept;

}  // namespace termchart::data

#endif  // TERMCHART_DATA_COLUMN_PROFILE_HPP
