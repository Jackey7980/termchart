// autoviz/router.hpp —— 六条管道路由（规格书阶段三，L21）
//
// 输入 DataFrame + DatasetFeatures，输出候选图列表（每个候选带惰性建图闭包与
// 打分元数据，不建图、不评分）。候选按管道优先级排列：Time > Geo > Event >
// CategoryNumeric > Numeric > Text > Preview；评分与截断在 engine。
#ifndef TERMCHART_AUTOVIZ_ROUTER_HPP
#define TERMCHART_AUTOVIZ_ROUTER_HPP

#include <vector>

#include "termchart/autoviz/candidate.hpp"
#include "termchart/autoviz/features.hpp"
#include "termchart/autoviz/options.hpp"
#include "termchart/data/dataframe.hpp"
#include "termchart/export.hpp"

namespace termchart::autoviz {

// 路由。永不返回空表——最不济也有 Preview 表格候选（"任何输入都有答案"）。
[[nodiscard]] TERMCHART_API std::vector<ChartCandidate> route(const data::DataFrame& frame,
                                                              const DatasetFeatures& features,
                                                              const AutoVizOptions& options = {});

}  // namespace termchart::autoviz

#endif  // TERMCHART_AUTOVIZ_ROUTER_HPP
