// autoviz/score.hpp —— 候选打分（规格书阶段四，L21）
//
// 两段式（对"全量沙盒光栅化"的刻意修正）：
//   1. analytic_score：纯元数据解析式打分——海选全部候选，零渲染成本；
//   2. raster_confirm：只对通过校验的 Top-N 做**画布级**光栅化复核
//      （建图 → Canvas → CellBuffer 统计墨迹/行使用率），O(N×画布) 只付一次。
// 覆盖率/遮挡率只当"决赛裁判"：糊满全屏的饼图覆盖率最高但最难看，所以
// 它不参与海选，只在复核里把"空"和"糊"两种失败形态挡掉。
#ifndef TERMCHART_AUTOVIZ_SCORE_HPP
#define TERMCHART_AUTOVIZ_SCORE_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "termchart/autoviz/candidate.hpp"
#include "termchart/autoviz/options.hpp"
#include "termchart/export.hpp"

namespace termchart::autoviz {

// 解析式打分（海选）。返回 [0,1]；否决候选得 0 分。
[[nodiscard]] TERMCHART_API double analytic_score(const ChartCandidate& candidate) noexcept;

// 画布级复核：建图 → Canvas(w,h) → CellBuffer 统计。返回 [0,1]；
// 建图失败/画布非法返回 0.0（该候选不应被选中）。
[[nodiscard]] TERMCHART_API double raster_confirm(const ChartCandidate& candidate,
                                                  std::uint32_t width,
                                                  std::uint32_t height) noexcept;

// 复核统计的明细（测试与诊断用）。
struct RasterStats {
    double coverage = 0.0;    // 非空格占比
    double row_usage = 0.0;   // 有墨迹的行占比
    std::size_t ink_cells = 0;
    std::size_t total_cells = 0;
};

[[nodiscard]] TERMCHART_API RasterStats raster_stats(const ChartCandidate& candidate,
                                                     std::uint32_t width,
                                                     std::uint32_t height);

}  // namespace termchart::autoviz

#endif  // TERMCHART_AUTOVIZ_SCORE_HPP
