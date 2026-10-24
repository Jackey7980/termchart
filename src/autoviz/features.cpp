// features.cpp —— 数据集特征探测实现
#include "termchart/autoviz/features.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace termchart::autoviz {
namespace {

bool equals_ci(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
        if (ca != cb) {
            return false;
        }
    }
    return true;
}

// 按名字找列下标；找不到返回 npos。
std::size_t find_column(const data::DataFrame& frame, std::string_view name) noexcept {
    for (std::size_t i = 0; i < frame.headers.size(); ++i) {
        if (frame.headers[i] == name) {
            return i;
        }
    }
    return static_cast<std::size_t>(-1);
}

bool contains(const std::vector<std::size_t>& v, std::size_t x) noexcept {
    return std::find(v.begin(), v.end(), x) != v.end();
}

void remove_from(std::vector<std::size_t>& v, std::size_t x) noexcept {
    v.erase(std::remove(v.begin(), v.end(), x), v.end());
}

}  // namespace

bool is_latitude_name(std::string_view name) noexcept {
    return equals_ci(name, "lat") || equals_ci(name, "latitude");
}

bool is_longitude_name(std::string_view name) noexcept {
    return equals_ci(name, "lon") || equals_ci(name, "lng") || equals_ci(name, "longitude");
}

DatasetFeatures detect_features(const data::DataFrame& frame, const AutoVizOptions& options) {
    DatasetFeatures f;
    f.profiles = data::profile_columns(frame, options.profile);
    f.rows = frame.row_count;

    // 1) 角色初分（顺序即列序，路由优先级有保障）。
    std::vector<std::size_t> lat_candidates;
    std::vector<std::size_t> lng_candidates;
    for (std::size_t i = 0; i < f.profiles.size(); ++i) {
        const data::ColumnProfile& p = f.profiles[i];
        if (p.kind == data::ColumnKind::NullOnly ||
            p.tier == data::CardinalityTier::Constant) {
            f.dropped.push_back(i);  // 常量/全空列：任何图都用不上
            continue;
        }
        switch (p.kind) {
        case data::ColumnKind::Timestamp:
            f.time_columns.push_back(i);
            break;
        case data::ColumnKind::Numeric:
            if (is_latitude_name(p.name)) {
                lat_candidates.push_back(i);
            } else if (is_longitude_name(p.name)) {
                lng_candidates.push_back(i);
            } else {
                f.numeric_columns.push_back(i);
            }
            break;
        case data::ColumnKind::Boolean:
        case data::ColumnKind::Text:
            if (p.tier == data::CardinalityTier::High) {
                f.text_columns.push_back(i);  // 自由文本：词云/预览
            } else {
                f.category_columns.push_back(i);  // Low/Mid：可分组/facet
            }
            break;
        case data::ColumnKind::Unknown:
        case data::ColumnKind::NullOnly:
            break;
        }
    }

    // 2) 分类列按"分组力"排序：distinct 升序（并列按列序）。
    //    事件管道/组合管道/地理实体都取 category_columns.front()——按列序盲取
    //    会让 order_id/customer_id 这类高 distinct 标识列当上事件键，画出
    //    "各 order_id 的均值"这种语义废图（真实脏订单数据的实测教训）。
    //    distinct 最小的列分组最粗、每组成员最多、聚合最稳；ID 类列只有在
    //    没有更好分类列时才被用上。显式 event_column 仍最高优先（见下）。
    std::sort(f.category_columns.begin(), f.category_columns.end(),
              [&](std::size_t a, std::size_t b) {
                  if (f.profiles[a].distinct != f.profiles[b].distinct) {
                      return f.profiles[a].distinct < f.profiles[b].distinct;
                  }
                  return a < b;
              });

    // 3) 经纬度配对：按列序取最早未配对的 lat/lng 成对；配不上的退回数值列。
    while (!lat_candidates.empty() && !lng_candidates.empty()) {
        f.geo_pairs.push_back(GeoPair{lat_candidates.front(), lng_candidates.front()});
        lat_candidates.erase(lat_candidates.begin());
        lng_candidates.erase(lng_candidates.begin());
    }
    for (const std::size_t i : lat_candidates) {
        f.numeric_columns.push_back(i);
    }
    for (const std::size_t i : lng_candidates) {
        f.numeric_columns.push_back(i);
    }

    // 4) 强制列指定（空 = 自动探测）。time_column 找到就置到队首；
    // event_column 只接受已判为分类的列（数值列当 event 没有分组语义）。
    if (!options.time_column.empty()) {
        const std::size_t idx = find_column(frame, options.time_column);
        if (idx != static_cast<std::size_t>(-1) && !contains(f.time_columns, idx)) {
            f.time_columns.insert(f.time_columns.begin(), idx);
        }
    }
    if (!options.event_column.empty()) {
        const std::size_t idx = find_column(frame, options.event_column);
        if (idx != static_cast<std::size_t>(-1) && contains(f.category_columns, idx)) {
            remove_from(f.category_columns, idx);
            f.category_columns.insert(f.category_columns.begin(), idx);
        }
    }

    // 4) 全局空值率剔除：**仅当没有 Low 档分类列（无 event 可分桶）时**生效。
    //    有 event 列时按桶看空值率（路由阶段用 null_rate_by_bucket），防止
    //    "全局 40% 空、return 桶里 0% 空"的关键字段被一刀切。
    if (options.drop_high_null) {
        const bool has_low_category =
            std::any_of(f.category_columns.begin(), f.category_columns.end(),
                        [&](std::size_t i) {
                            return f.profiles[i].tier == data::CardinalityTier::Low;
                        });
        if (!has_low_category) {
            auto drop_if_dirty = [&](std::vector<std::size_t>& list) {
                for (std::size_t i = 0; i < list.size();) {
                    if (f.profiles[list[i]].null_rate > options.max_null_rate) {
                        f.dropped.push_back(list[i]);
                        list.erase(list.begin() + static_cast<std::ptrdiff_t>(i));
                    } else {
                        ++i;
                    }
                }
            };
            drop_if_dirty(f.time_columns);
            drop_if_dirty(f.numeric_columns);
            drop_if_dirty(f.text_columns);
            drop_if_dirty(f.category_columns);
        }
    }
    return f;
}

}  // namespace termchart::autoviz
