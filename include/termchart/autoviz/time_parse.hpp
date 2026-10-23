// autoviz/time_parse.hpp —— ISO-8601 形状时间 → epoch 秒（L21）
//
// 为什么在 autoviz 而不在 data：data 层的 looks_like_timestamp 只判形状
// （路由够用），"解析成可排序可聚合的 epoch" 是管道路由的具体需求，且全库
// 没有第二处需要（data::ohlc 直接吃 epoch）。若将来 data 也要，下沉不迟。
//
// 口径（明确列出，避免"以为支持"）：
//   ✅ YYYY-MM-DD[ HH:MM[:SS[.fff]]]（空格或 T 分隔）
//   ✅ 后缀 Z 或 ±HH:MM / ±HHMM（按偏移换算成 UTC epoch）
//   ✅ 2 月 30 号这类非法日期**拒绝**（返回 nullopt）——与只认形状的
//      looks_like_timestamp 分工：那里管"像不像"，这里管"能不能算"。
//   ⛔ 不认时区别名（EST）、不认 Unix 时间戳数字串（那是数值列的活）、
//      不认 "Jan 5, 2024" 等非 ISO 形态。
#ifndef TERMCHART_AUTOVIZ_TIME_PARSE_HPP
#define TERMCHART_AUTOVIZ_TIME_PARSE_HPP

#include <optional>
#include <string_view>

#include "termchart/export.hpp"

namespace termchart::autoviz {

// 解析为 epoch 秒（double，1970-01-01T00:00:00Z 起）。无时区后缀按 UTC 解释
// （naive——可视化排序分桶不涉及时区显示，避免引入 tz 数据库依赖）。
[[nodiscard]] TERMCHART_API std::optional<double> parse_timestamp(std::string_view text) noexcept;

// 把 [t0, t1] 切成不超过 target_buckets 个等宽桶，返回桶宽（秒）。
// 桶宽取 1-2-5 阶梯上不小于区间/target 的最小档；t1<=t0 或非有限时返回 1.0。
[[nodiscard]] TERMCHART_API double nice_bucket_seconds(double t0, double t1,
                                                       std::size_t target_buckets) noexcept;

}  // namespace termchart::autoviz

#endif  // TERMCHART_AUTOVIZ_TIME_PARSE_HPP
