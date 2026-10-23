// autoviz/semantic.hpp —— 一票否决层（规格书阶段四的硬约束，L21）
//
// 在沙盒打分**之前**执行：不合格的候选直接出局，根本不值得光栅化。
// 每条否决都返回**结构化理由**（规则码: 人话）——静默丢弃候选是排障噩梦，
// 也与全库 fail-loud 文化一致。
#ifndef TERMCHART_AUTOVIZ_SEMANTIC_HPP
#define TERMCHART_AUTOVIZ_SEMANTIC_HPP

#include <string>
#include <vector>

#include "termchart/autoviz/candidate.hpp"
#include "termchart/export.hpp"

namespace termchart::autoviz {

// 返回空 vector = 通过；否则每条是 "规则码: 说明"。
[[nodiscard]] TERMCHART_API std::vector<std::string> semantic_check(const ChartCandidate& candidate);

}  // namespace termchart::autoviz

#endif  // TERMCHART_AUTOVIZ_SEMANTIC_HPP
