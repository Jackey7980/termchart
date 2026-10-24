#ifndef TERMCHART_SRC_DATA_CELL_FLAGS_INTERNAL_HPP
#define TERMCHART_SRC_DATA_CELL_FLAGS_INTERNAL_HPP

// 私有头（不进 include/，故不参与 check_headers / header_only 判定）：
// ScalarFlag → CellFlag 的**唯一**映射点。
//
// 为什么必须共用：CSV 轨和 JSONL 轨是同一套清洗口径的两条入口。JSON 轨曾经
// 完全没做这一步，于是 "15%" 的 Percent、全角数字的 FullWidth、货币的 Currency
// 在 DataFrame 里凭空消失——下游列剖析看不到量纲差异，把 0.15 当成和 1234.5
// 同质的金额。同一个事实只有一个转换函数，是这个文件存在的全部理由。

#include "termchart/data/dataframe.hpp"
#include "termchart/data/scalar_parse.hpp"

namespace termchart::data::detail {

inline CellFlag map_scalar_flags(ScalarFlag f) noexcept {
    CellFlag out = CellFlag::None;
    if (has_flag(f, ScalarFlag::HasComma)) out = out | CellFlag::HasComma;
    if (has_flag(f, ScalarFlag::AccountingNegative)) out = out | CellFlag::AccountingNegative;
    if (has_flag(f, ScalarFlag::Percent)) out = out | CellFlag::Percent;
    if (has_flag(f, ScalarFlag::FullWidth)) out = out | CellFlag::FullWidth;
    if (has_flag(f, ScalarFlag::Currency)) out = out | CellFlag::Currency;
    return out;
}

}  // namespace termchart::data::detail

#endif  // TERMCHART_SRC_DATA_CELL_FLAGS_INTERNAL_HPP
