// autoviz/options.hpp —— AutoViz 入口选项与管道枚举（L21）
//
// 定位：autoviz 是"脏数据 → 选对图"的编排层（规格书四阶段 + 一校验）。
// 本文件只放选项与枚举，不含逻辑；画像口径复用 data 层（L4），不重造。
#ifndef TERMCHART_AUTOVIZ_OPTIONS_HPP
#define TERMCHART_AUTOVIZ_OPTIONS_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "termchart/data/column_profile.hpp"
#include "termchart/export.hpp"

namespace termchart::autoviz {

// 六条处理管道（规格书阶段三）。Preview 是兜底：什么都画不了时出数据表格。
enum class PipelineKind : std::uint8_t {
    Time,             // 时间管道：ts + 数值 → 折线 / 时间×分类热力图
    Geo,              // 地理管道：lat/lng → 散点 / Hexbin（强制去重）
    Event,            // 事件/分类管道：低基数 event → 柱状 / 箱线 / 饼
    CategoryNumeric,  // 分类+数值组合：分类做颜色，数值做坐标/大小
    Text,             // 纯文本管道：词云
    Numeric,          // 纯数值管道：1列直方图、2列散点、多列平行坐标/相关矩阵
    Preview,          // 兜底：数据预览表格
};

[[nodiscard]] const char* pipeline_name(PipelineKind kind) noexcept;

// AutoViz 选项。默认值对应规格书；探测口径全部复用 data::ProfileOptions
// （含 CardinalityPolicy——基数分档阈值是策略值，调用方可覆盖）。
struct AutoVizOptions {
    // 特征探测（data 层画像）
    data::ProfileOptions profile{};

    // 脏数据剔除：**仅当没有低基数 event 列可分桶时**才做全局空值率剔除
    // （规格书修正案：有 event 列时按桶看空值率，防止误杀事件关键字段）。
    bool drop_high_null = true;
    double max_null_rate = 0.6;
    std::size_t min_rows = 2;  // 少于此行数不出数值图（Preview 兜底）

    // 强制指定列（空 = 自动探测）
    std::string_view event_column;
    std::string_view time_column;

    // 沙盒评估（规格书阶段四）
    std::size_t max_candidates = 6;       // 进入评分的候选上限
    std::size_t raster_confirm_top = 2;   // 只对前 N 名做画布级光栅化复核
    std::uint32_t canvas_width = 80;      // 复核画布（与 api 默认出图一致）
    std::uint32_t canvas_height = 24;
};

}  // namespace termchart::autoviz

#endif  // TERMCHART_AUTOVIZ_OPTIONS_HPP
