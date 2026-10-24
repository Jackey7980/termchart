// autoviz/features.hpp —— 数据集特征探测（规格书阶段二在 autoviz 侧落地，L21）
//
// 在 data::ColumnProfile（列级画像）之上做**角色分配**：哪列是时间、哪两列
// 是经纬度、哪列是可分组的分类、哪列是自由文本、哪些列该丢。探测本身极克制：
// 复用 data 层的单遍画像 + 熔断，本文件只做归组，不再遍历数据。
#ifndef TERMCHART_AUTOVIZ_FEATURES_HPP
#define TERMCHART_AUTOVIZ_FEATURES_HPP

#include <cstddef>
#include <string_view>
#include <vector>

#include "termchart/autoviz/domain.hpp"
#include "termchart/autoviz/options.hpp"
#include "termchart/data/column_profile.hpp"
#include "termchart/data/dataframe.hpp"
#include "termchart/export.hpp"

namespace termchart::autoviz {

// 经纬度列对（lat/lng 各一列，按列序配对）。
struct GeoPair {
    std::size_t lat = 0;
    std::size_t lng = 0;
};

// 列名语义 vs 推断类型的体检提示（**仅诊断，绝不改路由**）。
// 名字像度量（amount/price/total/revenue/...）却推断成 Text，或名字像分类
// （method/status/channel/...）却推断成 Numeric——典型成因是表头与数据列序
// 错位（如 amount/method 两列写反）。引擎照类型路由仍能画对图，但调用方
// （人）应当看见这条提示去修数据，而不是对着一张口径不一的车目瞪口呆。
struct ColumnSuspect {
    std::string name;
    std::string expected;  // "numeric" / "categorical"（名字暗示的口径）
    std::string actual;    // 推断口径（column_kind_name）
};

// 量级离群（**仅诊断，绝不改路由**）。与 ColumnSuspect 分开：那个管"列名与
// 类型矛盾"，这个管"值域里混进哨兵/单位错/手滑多打了 0"。典型后果不是选错图，
// 而是轴域被单个值撑爆、其余数据压成平线——所以既要告警，又要裁轴（见
// autoviz/domain.hpp）。抽样口径：按行等距抽样（不是前 N 行——离群值常在尾部）。
struct ValueOutlier {
    std::string name;
    std::size_t sampled = 0;  // 参与判断的有效值数（抽样的非空非 NaN 值）
    std::size_t count = 0;    // 越界值个数（抽样口径）
    double median = 0.0;      // 主体所在
    double extreme = 0.0;     // 越界最远的实际值
    bool high = true;         // 上溢（脏大数）还是下溢（脏负数）
    // 轴域怎么处理的：kApplied 才是真裁过；kSmallSample / kTooMany 表示检测
    // 到了脏值但小样本保护闸放弃了裁剪（轴域仍是原始范围）——告警文案据此
    // 区分"已裁剪"与"保持原样"，不许混为一谈。
    ClipDecision decision = ClipDecision::kApplied;
};

// 数据集特征：列画像 + 角色分组 + 剔除名单。
struct DatasetFeatures {
    std::vector<data::ColumnProfile> profiles;  // 与 DataFrame.columns 同序对齐
    std::vector<std::size_t> time_columns;
    std::vector<GeoPair> geo_pairs;
    std::vector<std::size_t> category_columns;  // Low/Mid 档分类列（可 facet）
    std::vector<std::size_t> numeric_columns;
    std::vector<std::size_t> text_columns;      // High 档自由文本（词云用）
    std::vector<std::size_t> dropped;           // 常量/全空/高空值率被剔除
    std::vector<ColumnSuspect> suspects;        // 表头口径存疑列（诊断用）
    std::vector<ValueOutlier> outliers;         // 量级离群列（诊断用）
    std::size_t rows = 0;

    // 便捷视图
    [[nodiscard]] const data::ColumnProfile& profile(std::size_t index) const noexcept {
        return profiles[index];
    }
    [[nodiscard]] std::size_t column_count() const noexcept { return profiles.size(); }
    [[nodiscard]] bool has_time() const noexcept { return !time_columns.empty(); }
    [[nodiscard]] bool has_geo() const noexcept { return !geo_pairs.empty(); }
    [[nodiscard]] bool has_category() const noexcept { return !category_columns.empty(); }
    [[nodiscard]] bool has_numeric() const noexcept { return !numeric_columns.empty(); }
    [[nodiscard]] bool has_text() const noexcept { return !text_columns.empty(); }
};

// 特征探测。规则（与规格书阶段二对应）：
//   * Timestamp → time_columns；
//   * Numeric 且列名命中 lat/latitude、lon/lng/longitude → 尝试配成 GeoPair，
//     配不上的留在 numeric_columns；
//   * Text/Boolean：Low/Mid 档 → category_columns（**按 distinct 升序排列**——
//     取 front() 的管道因此拿到"分组力最强"的列而非列序第一列，防止
//     order_id 这类高 distinct 标识列当上事件键），High 档 → text_columns；
//   * Constant（distinct≤1）或 NullOnly → dropped；
//   * **仅当不存在 Low 档分类列时**（没有 event 列可分桶），才对其余列应用
//     全局空值率剔除（drop_high_null / max_null_rate）；
//   * 列名口径体检：度量词却是 Text / 分类词却是 Numeric（且非 0/1 flag）
//     → suspects（**只诊断，绝不改路由**）。
//   * 量级离群体检：数值列等距抽样后按 IQR 篱判越界 → outliers（同样只诊断；
//     真正的轴域裁剪在路由阶段用全量值重算，见 autoviz/domain.hpp）。
[[nodiscard]] TERMCHART_API DatasetFeatures detect_features(const data::DataFrame& frame,
                                                             const AutoVizOptions& options = {});

// 列名是否是经纬度语义（大小写不敏感）。lat：lat/latitude；lng：lon/lng/longitude。
[[nodiscard]] TERMCHART_API bool is_latitude_name(std::string_view name) noexcept;
[[nodiscard]] TERMCHART_API bool is_longitude_name(std::string_view name) noexcept;

}  // namespace termchart::autoviz

#endif  // TERMCHART_AUTOVIZ_FEATURES_HPP
