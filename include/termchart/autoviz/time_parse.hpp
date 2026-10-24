// autoviz/time_parse.hpp —— 时间形状 → epoch 秒（L21）
//
// 与 data::split_timestamp_shape 的分工（**唯一真源在 data 层，此处不重复
// 实现格式族**）：那里管"像不像时间"（列画像的类型判定，只认形状不验历法），
// 这里管"能不能算"（历法校验 + epoch 换算）。支持族见 data/column_profile.hpp
// 的 split_timestamp_shape 文档，要点：
//   ✅ YYYY[-/./年]M[-/./月]D[ T日][H[:时]M[:分]S[.fff]]][Z|±HH[:MM]]
//   ✅ 月/日/时/分/秒 1-2 位（真实导出常不补零）；
//   ✅ 2 月 30 号、2026/13/40 这类**形状合法但历法非法 → 拒绝**（nullopt）；
//   ⛔ Unix 时间戳数字串（1740787200）刻意不认——与普通大整数不可区分，
//      要时序图请显式 opts.time_column 或先用 data 层转好。
#ifndef TERMCHART_AUTOVIZ_TIME_PARSE_HPP
#define TERMCHART_AUTOVIZ_TIME_PARSE_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "termchart/export.hpp"

namespace termchart::autoviz {

// 解析为 epoch 秒（double，1970-01-01T00:00:00Z 起；时区后缀按偏移换算，
// 无后缀按 UTC 解释——naive，不做时区显示，避免引入 tz 数据库依赖）。
[[nodiscard]] TERMCHART_API std::optional<double> parse_timestamp(std::string_view text) noexcept;

// 把 [t0, t1] 切成不超过 target_buckets 个等宽桶，返回桶宽（秒）。
// 桶宽取 1-2-5 阶梯上不小于区间/target 的最小档；t1<=t0 或非有限时返回 1.0。
[[nodiscard]] TERMCHART_API double nice_bucket_seconds(double t0, double t1,
                                                       std::size_t target_buckets) noexcept;

// epoch 秒 → 轴刻度短标签（parse_timestamp 的逆方向，**UTC/naive 口径**——
// 与解析口径一致，不做本地时区换算：否则同一份数据在不同机器上显示不同时间）。
// 粒度不能只看总跨度：跨度 1.7 天而刻度间隔 13 小时时，"MM-DD" 会让相邻两个
// 刻度都显示 "03-01"（信息为零）。所以两参数各管一段：
//   span < 1 天                        → "HH:MM"        （同一天内，无歧义）
//   否则 resolution < 1 天              → "MM-DD HH:MM"  （跨天但需要小时精度）
//   否则 span < 2 年                    → "MM-DD"
//   否则                                → "YYYY-MM"
// 非有限 epoch 返回 "?"。
[[nodiscard]] TERMCHART_API std::string
format_timestamp_short(double epoch, double span_seconds, double resolution_seconds) noexcept;

// 时长 → 人类可读（"2 小时" / "90 分钟" / "7 天"），用于图表标题的桶宽描述。
[[nodiscard]] TERMCHART_API std::string format_span_human(double seconds) noexcept;

}  // namespace termchart::autoviz

#endif  // TERMCHART_AUTOVIZ_TIME_PARSE_HPP
