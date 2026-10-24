// features.cpp —— 数据集特征探测实现
#include "termchart/autoviz/features.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <string_view>

#include "termchart/autoviz/domain.hpp"

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

// —— 列名语义体检（只诊断，不改路由）——
// 列名按"词"匹配：非字母数字（下划线/空格/点/连字符/中文）都当分隔符，
// camelCase 也拆（totalAmount → total/amount），全小写后逐词比对，末尾复数 s
// 视同单数（fees → fee）。用词而非子串：account 不会被 count 命中。
bool is_ascii_alnum(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

char ascii_lower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

std::vector<std::string> name_tokens(std::string_view name) {
    std::vector<std::string> tokens;
    std::string cur;
    auto flush = [&] {
        if (!cur.empty()) {
            tokens.push_back(cur);
            cur.clear();
        }
    };
    for (std::size_t i = 0; i < name.size(); ++i) {
        const char c = name[i];
        if (!is_ascii_alnum(c)) {
            flush();
            continue;
        }
        // camelCase / PascalCase 拆词：小写或数字后面紧跟大写即边界。
        if (!cur.empty() && c >= 'A' && c <= 'Z') {
            const char prev = name[i - 1];
            if ((prev >= 'a' && prev <= 'z') || (prev >= '0' && prev <= '9')) {
                flush();
            }
        }
        cur.push_back(ascii_lower(c));
    }
    flush();
    return tokens;
}

bool token_is(const std::string& token, std::string_view word) noexcept {
    if (token.size() == word.size()) {
        for (std::size_t i = 0; i < token.size(); ++i) {
            if (token[i] != word[i]) {
                return false;
            }
        }
        return true;
    }
    // 复数：fees → fee
    if (token.size() == word.size() + 1 && token.back() == 's') {
        return token.compare(0, word.size(), word.data(), word.size()) == 0;
    }
    return false;
}

bool name_has_any(std::string_view name, std::initializer_list<std::string_view> words) {
    const std::vector<std::string> tokens = name_tokens(name);
    for (const std::string& token : tokens) {
        for (const std::string_view word : words) {
            if (token_is(token, word)) {
                return true;
            }
        }
    }
    return false;
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

    // 5) 全局空值率剔除：**仅当没有 Low 档分类列（无 event 可分桶）时**生效。
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

    // 6) 列名语义体检（**只诊断，绝不改路由**）：名字口径与推断类型互相矛盾的
    //    列记进 suspects。典型成因是表头与数据列序错位（amount/method 两列写反），
    //    引擎信类型路由仍能画出语义正确的图，但人必须看见提示去修数据，而不是
    //    对着 "各 amount 的 method 均值" 这种标题发呆。
    //    刻意不报的情形：分类名 + 数值只有 0/1 → flag 编码（status 常见写法），
    //    归 Numeric 是正常口径而非错位；名字同时命中两张词表时以分类词为准
    //    （payment_method 是维度，不是度量）。
    for (const data::ColumnProfile& p : f.profiles) {
        const bool categorical_named =
            name_has_any(p.name, {"method", "gateway", "currency", "channel", "category",
                                  "brand", "vendor", "country", "city", "region"});
        if (p.kind == data::ColumnKind::Numeric && categorical_named) {
            const bool flag_like = p.min >= 0.0 && p.max <= 1.0;
            if (!flag_like) {
                f.suspects.push_back(ColumnSuspect{p.name, "categorical",
                                                   data::column_kind_name(p.kind)});
            }
            continue;
        }
        if (p.kind == data::ColumnKind::Text && !categorical_named &&
            name_has_any(p.name, {"amount", "price", "total", "subtotal", "revenue", "income",
                                  "cost", "fee", "tax", "sum", "balance", "qty", "quantity",
                                  "count", "weight", "score", "salary", "profit", "budget",
                                  "discount"})) {
            f.suspects.push_back(ColumnSuspect{p.name, "numeric",
                                               data::column_kind_name(p.kind)});
        }
    }

    // 7) 量级离群体检（同样**只诊断**）：按行**等距抽样**扫数值列，用 IQR 篱
    //    判"有没有被个别脏值撑爆"。刻意不取前 N 行——脏行常在尾部（手滑追加、
    //    汇总行混入），只看开头必然漏。抽样上限 1024 个值，够撑起分位数且对
    //    百万行表也是 O(1e3)。
    //    这里的 count 是**抽样口径**，路由阶段会在真正建图时用全量值重算一次
    //    精确的裁剪数（填 ChartCandidate::clipped_points）。
    if (options.clip_outliers) {
        constexpr std::size_t kSampleCap = 1024;
        for (const std::size_t idx : f.numeric_columns) {
            const std::vector<data::Cell>& column = frame.columns[idx];
            if (column.empty()) {
                continue;
            }
            const std::size_t stride = std::max<std::size_t>(1, column.size() / kSampleCap);
            std::vector<double> sample;
            sample.reserve(std::min(column.size(), kSampleCap) + 1);
            for (std::size_t r = 0; r < column.size(); r += stride) {
                // 与路由的量纲闸同一判据：百分比混进金额列时，那个 0.15 不是"极小值"，
                // 是**另一个量纲**。把它算进样本会既污染分位数、又在告警里把它说成
                // "极端离群值（0.15 远小于中位数 1100）"——而图上早就不画它了。
                const bool other_unit =
                    f.profiles[idx].mixed_units &&
                    data::has_cell_flag(column[r].flags, data::CellFlag::Percent);
                if (column[r].is_numeric() && std::isfinite(column[r].num) && !other_unit) {
                    sample.push_back(column[r].num);
                }
            }
            const RobustDomain dom = robust_y_domain(sample, options.outlier_fence);
            // 判据是"检测到越界"（clipped>0），不是"真裁了轴"（active()）：
            // 小样本保护闸放弃裁剪时更该告警——此时图上是原始域，顶端那片
            // 贴边平台是真值还是脏值，只有告警能说清。这里只负责**报告检测事实**；
            // "最终到底裁没裁"的唯一口径在 ChartCandidate::final_clip_desc（由路由
            // 用全量值重算），api 层只播报那一份，不许再按 decision 另起一套说法。
            if (dom.clipped == 0) {
                continue;
            }
            f.outliers.push_back(ValueOutlier{f.profiles[idx].name, sample.size(), dom.clipped,
                                              dom.median, dom.extreme(), dom.high, dom.decision});
        }
    }
    return f;
}

}  // namespace termchart::autoviz
