// column_profile.cpp —— 列画像实现（单遍 + 熔断 + 抽样）
#include "termchart/data/column_profile.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

namespace termchart::data {
namespace {

bool is_digit(char ch) noexcept {
    return ch >= '0' && ch <= '9';
}

bool two_digits(std::string_view text, std::size_t at) noexcept {
    return at + 1 < text.size() && is_digit(text[at]) && is_digit(text[at + 1]);
}

// 列名：headers 缺位时补 colN（与 DataFrame 无表头命名口径一致）。
std::string column_name_at(const std::vector<std::string>& headers, std::size_t index) {
    if (index < headers.size()) {
        return headers[index];
    }
    return "col" + std::to_string(index);
}

}  // namespace

const char* cardinality_tier_name(CardinalityTier tier) noexcept {
    switch (tier) {
    case CardinalityTier::Constant: return "constant";
    case CardinalityTier::Low:      return "low";
    case CardinalityTier::Mid:      return "mid";
    case CardinalityTier::High:     return "high";
    }
    return "?";
}

bool looks_like_timestamp(std::string_view text) noexcept {
    if (text.size() < 10) {
        return false;
    }
    for (std::size_t i = 0; i < 4; ++i) {
        if (!is_digit(text[i])) return false;
    }
    if (text[4] != '-' || text[7] != '-') return false;
    if (!two_digits(text, 5) || !two_digits(text, 8)) return false;
    if (text.size() == 10) {
        return true;  // 纯日期
    }
    std::size_t i = 10;
    if (text[i] != ' ' && text[i] != 'T') return false;
    ++i;
    if (!two_digits(text, i)) return false;
    i += 2;
    if (i >= text.size() || text[i] != ':') return false;
    ++i;
    if (!two_digits(text, i)) return false;
    i += 2;
    if (i < text.size() && text[i] == ':') {  // 可选秒
        ++i;
        if (!two_digits(text, i)) return false;
        i += 2;
    }
    if (i < text.size() && text[i] == '.') {  // 可选毫秒
        ++i;
        std::size_t digits = 0;
        while (i < text.size() && is_digit(text[i])) {
            ++i;
            ++digits;
        }
        if (digits == 0) return false;
    }
    if (i < text.size()) {  // 可选时区
        if (text[i] == 'Z') {
            ++i;
        } else if (text[i] == '+' || text[i] == '-') {
            ++i;
            if (!two_digits(text, i)) return false;
            i += 2;
            if (i < text.size() && text[i] == ':') ++i;
            if (i < text.size()) {
                if (!two_digits(text, i)) return false;
                i += 2;
            }
        } else {
            return false;
        }
    }
    return i == text.size();
}

bool looks_like_boolean(std::string_view text) noexcept {
    if (text.empty() || text.size() > 5) {
        return false;
    }
    char lower[6];
    for (std::size_t i = 0; i < text.size(); ++i) {
        char ch = text[i];
        lower[i] = static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch);
    }
    lower[text.size()] = '\0';
    const std::string_view word(lower, text.size());
    return word == "true" || word == "false" || word == "yes" || word == "no";
}

std::vector<ColumnProfile> profile_columns(const DataFrame& frame, const ProfileOptions& options) {
    std::vector<ColumnProfile> profiles;
    profiles.reserve(frame.columns.size());

    for (std::size_t c = 0; c < frame.columns.size(); ++c) {
        const std::vector<Cell>& column = frame.columns[c];
        ColumnProfile profile;
        profile.name = column_name_at(frame.headers, c);
        profile.rows = column.size();

        // 单遍：非空/类型抽样/极值/单调/distinct 熔断一次走完。
        std::size_t numeric_count = 0;
        std::size_t sample_non_null = 0;
        std::size_t sample_numeric = 0;
        std::size_t sample_timestamp = 0;
        std::size_t sample_boolean = 0;
        std::unordered_set<std::string> distinct;
        bool monotonic = true;
        bool have_prev = false;
        double prev = 0.0;
        const std::size_t sample_end =
            options.sample_rows == 0 ? column.size()
                                     : std::min(column.size(), options.sample_rows);

        for (std::size_t r = 0; r < column.size(); ++r) {
            const Cell& cell = column[r];
            if (cell.is_missing()) {
                continue;
            }
            ++profile.non_null;

            if (r < sample_end) {  // 类型判定只信抽样
                ++sample_non_null;
                if (cell.is_numeric()) {
                    ++sample_numeric;
                } else if (looks_like_timestamp(cell.raw)) {
                    ++sample_timestamp;
                }
                if (looks_like_boolean(cell.raw)) {
                    ++sample_boolean;
                }
            }

            if (cell.is_numeric() && !std::isnan(cell.num)) {
                if (numeric_count == 0) {
                    profile.min = cell.num;
                    profile.max = cell.num;
                } else {
                    profile.min = std::min(profile.min, cell.num);
                    profile.max = std::max(profile.max, cell.num);
                    if (cell.num < prev) {
                        monotonic = false;
                    }
                }
                prev = cell.num;
                have_prev = true;
                ++numeric_count;
            }

            if (!profile.distinct_capped) {  // 熔断：超 cap 立即停，不再喂集合
                distinct.insert(cell.raw);
                if (distinct.size() > options.distinct_cap) {
                    profile.distinct_capped = true;
                }
            }
        }

        if (numeric_count == 0) {
            profile.min = std::numeric_limits<double>::quiet_NaN();
            profile.max = std::numeric_limits<double>::quiet_NaN();
        } else {
            profile.monotonic_nondecreasing = monotonic && have_prev;
        }
        if (profile.rows > 0) {
            profile.null_rate = 1.0 - static_cast<double>(profile.non_null) /
                                          static_cast<double>(profile.rows);
        }
        profile.distinct = distinct.size();
        profile.tier = tier_of(profile.distinct, options.cardinality);

        // 类型判定（占比阈值；布尔要求全票，避免把"true/3/9"这种混列算布尔）。
        if (profile.rows == 0) {
            profile.kind = ColumnKind::Unknown;
        } else if (profile.non_null == 0) {
            profile.kind = ColumnKind::NullOnly;
        } else if (sample_non_null > 0) {
            const auto ratio = [&](std::size_t hit) {
                return static_cast<double>(hit) / static_cast<double>(sample_non_null);
            };
            if (ratio(sample_timestamp) >= options.kind_ratio) {
                profile.kind = ColumnKind::Timestamp;
            } else if (ratio(sample_numeric) >= options.kind_ratio) {
                profile.kind = ColumnKind::Numeric;
            } else if (ratio(sample_boolean) >= 1.0) {
                profile.kind = ColumnKind::Boolean;
            } else {
                profile.kind = ColumnKind::Text;
            }
        }

        profiles.push_back(std::move(profile));
    }
    return profiles;
}

std::vector<BucketNullRate> null_rate_by_bucket(const DataFrame& frame,
                                                std::string_view key_column,
                                                std::string_view value_column) {
    std::vector<BucketNullRate> out;
    const auto key_at = std::find(frame.headers.begin(), frame.headers.end(), key_column);
    const auto value_at = std::find(frame.headers.begin(), frame.headers.end(), value_column);
    if (key_at == frame.headers.end() || value_at == frame.headers.end()) {
        return out;  // 列不存在 → 空结果（调用方按"无 event 列"退化全局口径）
    }
    const std::size_t key_index = static_cast<std::size_t>(key_at - frame.headers.begin());
    const std::size_t value_index = static_cast<std::size_t>(value_at - frame.headers.begin());
    if (key_index >= frame.columns.size() || value_index >= frame.columns.size()) {
        return out;
    }
    const std::vector<Cell>& keys = frame.columns[key_index];
    const std::vector<Cell>& values = frame.columns[value_index];
    const std::size_t rows = std::min(keys.size(), values.size());

    std::vector<std::pair<std::string, BucketNullRate>> buckets;  // 保序累积，末尾排序
    auto bucket_of = [&buckets](const std::string& key) -> BucketNullRate& {
        for (auto& [name, bucket] : buckets) {
            if (name == key) {
                return bucket;
            }
        }
        buckets.emplace_back(key, BucketNullRate{key, 0, 0, 0.0});
        return buckets.back().second;
    };

    for (std::size_t r = 0; r < rows; ++r) {
        if (keys[r].is_missing()) {
            continue;  // 无法归属的行跳过
        }
        BucketNullRate& bucket = bucket_of(keys[r].raw);
        ++bucket.rows;
        if (!values[r].is_missing()) {
            ++bucket.non_null;
        }
    }
    for (auto& [name, bucket] : buckets) {
        bucket.null_rate = bucket.rows == 0
                               ? 0.0
                               : 1.0 - static_cast<double>(bucket.non_null) /
                                             static_cast<double>(bucket.rows);
    }
    out.reserve(buckets.size());
    for (auto& entry : buckets) {
        out.push_back(std::move(entry.second));
    }
    std::sort(out.begin(), out.end(), [](const BucketNullRate& a, const BucketNullRate& b) {
        return a.key < b.key;
    });
    return out;
}

}  // namespace termchart::data
