// autoviz/engine.hpp —— AutoViz 入口（规格书四阶段编排，L21）
//
// 流程：detect_features（阶段二）→ route（阶段三）→ semantic_check（阶段四
// 硬约束）→ analytic_score（海选）→ raster_confirm Top-N（决赛）→ 终排序。
// 单遍 O(N) 只发生在探测与管道聚合；渲染打分只对 Top-N 付一次画布成本。
#ifndef TERMCHART_AUTOVIZ_ENGINE_HPP
#define TERMCHART_AUTOVIZ_ENGINE_HPP

#include <memory>
#include <string_view>
#include <vector>

#include "termchart/autoviz/candidate.hpp"
#include "termchart/autoviz/features.hpp"
#include "termchart/autoviz/options.hpp"
#include "termchart/core/expected.hpp"
#include "termchart/data/dataframe.hpp"
#include "termchart/export.hpp"

namespace termchart::autoviz {

struct AutoVizResult {
    DatasetFeatures features;
    std::vector<ChartCandidate> candidates;  // 已打分；按 total_score 降序，否决的沉底

    // 最高分的可用候选（无可用候选时 nullptr——调用方应退化到表格或报错）。
    [[nodiscard]] const ChartCandidate* chosen() const noexcept;

    // 惰性建出选中图（chosen() 为空或建图失败时返回 nullptr）。
    [[nodiscard]] std::unique_ptr<chart::Chart> build_chosen() const;
};

// 主入口：DataFrame → 选中图 + 全部候选评分明细。
// 空表（0 列）返回 InvalidArgument——"没有数据"不是可视化问题。
[[nodiscard]] TERMCHART_API Result<AutoVizResult> auto_visualize(
    const data::DataFrame& frame, const AutoVizOptions& options = {});

// JSONL 一站式入口：解析（保 Key、脏行跳过计数）→ auto_visualize。
[[nodiscard]] TERMCHART_API Result<AutoVizResult> auto_visualize_jsonl(
    std::string_view jsonl, const AutoVizOptions& options = {});

}  // namespace termchart::autoviz

#endif  // TERMCHART_AUTOVIZ_ENGINE_HPP
