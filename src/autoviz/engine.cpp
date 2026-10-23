// engine.cpp —— AutoViz 编排实现
#include "termchart/autoviz/engine.hpp"

#include <algorithm>
#include <cmath>

#include "termchart/autoviz/features.hpp"
#include "termchart/autoviz/router.hpp"
#include "termchart/autoviz/score.hpp"
#include "termchart/autoviz/semantic.hpp"
#include "termchart/data/dataframe_json.hpp"

namespace termchart::autoviz {
namespace {

// 排序键：可用候选按综合分降序；否决的一律沉底（分再高也不选）。
bool better(const ChartCandidate& a, const ChartCandidate& b) noexcept {
    if (a.viable() != b.viable()) {
        return a.viable();
    }
    if (!a.viable()) {
        return false;  // 都不可用：保持路由顺序（确定性）
    }
    if (std::isnan(a.raster_score) != std::isnan(b.raster_score)) {
        return !std::isnan(a.raster_score);  // 复核过的排前（信息更全）
    }
    const double sa = a.total_score();
    const double sb = b.total_score();
    if (sa != sb) {
        return sa > sb;
    }
    return a.name < b.name;  // 同分按图种名（确定性）
}

}  // namespace

const ChartCandidate* AutoVizResult::chosen() const noexcept {
    for (const ChartCandidate& c : candidates) {
        if (c.viable()) {
            return &c;
        }
    }
    return nullptr;
}

std::unique_ptr<chart::Chart> AutoVizResult::build_chosen() const {
    const ChartCandidate* pick = chosen();
    if (pick == nullptr || !pick->build) {
        return nullptr;
    }
    try {
        return pick->build();
    } catch (...) {
        return nullptr;
    }
}

Result<AutoVizResult> auto_visualize(const data::DataFrame& frame,
                                     const AutoVizOptions& options) {
    if (frame.columns.empty()) {
        return Result<AutoVizResult>(Unexpected<ErrorCode>(ErrorCode::InvalidArgument));
    }
    AutoVizResult result;
    result.features = detect_features(frame, options);
    result.candidates = route(frame, result.features, options);

    // 阶段四硬约束：先否决，再打分（否决的不值得打分）。
    for (ChartCandidate& c : result.candidates) {
        c.vetoes = semantic_check(c);
    }
    for (ChartCandidate& c : result.candidates) {
        c.analytic_score = analytic_score(c);
    }

    // 海选排序 → Top-N 光格化复核 → 终排序。
    std::stable_sort(result.candidates.begin(), result.candidates.end(), better);
    std::size_t confirmed = 0;
    for (ChartCandidate& c : result.candidates) {
        if (!c.viable()) {
            continue;
        }
        if (confirmed >= options.raster_confirm_top) {
            break;
        }
        c.raster_score = raster_confirm(c, options.canvas_width, options.canvas_height);
        ++confirmed;
    }
    std::stable_sort(result.candidates.begin(), result.candidates.end(), better);
    return result;
}

Result<AutoVizResult> auto_visualize_jsonl(std::string_view jsonl,
                                           const AutoVizOptions& options) {
    const Result<data::JsonFrameResult> parsed = data::dataframe_from_jsonl(jsonl);
    if (!parsed.has_value()) {
        return Result<AutoVizResult>(Unexpected<ErrorCode>(parsed.error()));
    }
    return auto_visualize(parsed->frame, options);
}

}  // namespace termchart::autoviz
