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

// 多字节 UTF-8 分隔符（年月日时分秒）eat 助手。
bool eat_sep(std::string_view text, std::size_t& i, std::string_view sep) noexcept {
    if (text.compare(i, sep.size(), sep) != 0) {
        return false;
    }
    i += sep.size();
    return true;
}

// 1-2 位数字（真实导出常不补零："2026年3月2日"）。
bool one_or_two_digits(std::string_view text, std::size_t& i, unsigned& out) noexcept {
    if (i >= text.size() || !is_digit(text[i])) {
        return false;
    }
    unsigned value = 0;
    int count = 0;
    while (i < text.size() && is_digit(text[i]) && count < 2) {
        value = value * 10 + static_cast<unsigned>(text[i] - '0');
        ++i;
        ++count;
    }
    out = value;
    return true;
}

// 列名：headers 缺位时补 colN（与 DataFrame 无表头命名口径一致）。
std::string column_name_at(const std::vector<std::string>& headers, std::size_t index) {
    if (index < headers.size()) {
        return headers[index];
    }
    return "col" + std::to_string(index);
}

}  // namespace

bool split_timestamp_shape(std::string_view text, CivilParts& out) noexcept {
    out = CivilParts{};
    if (text.size() < 8) {
        return false;  // 最短 "2026-1-1"
    }
    std::size_t i = 0;

    // 年：严格 4 位。
    std::int64_t year = 0;
    for (int k = 0; k < 4; ++k) {
        if (i >= text.size() || !is_digit(text[i])) {
            return false;
        }
        year = year * 10 + (text[i] - '0');
        ++i;
    }

    // 日期分隔符族：- / . 年 月（两侧可混用，容忍即可，无副作用）。
    static constexpr std::string_view kDateSeps[] = {"-", "/", ".", "\xE5\xB9\xB4",
                                                     "\xE6\x9C\x88"};
    bool matched = false;
    for (const std::string_view sep : kDateSeps) {
        if (eat_sep(text, i, sep)) {
            matched = true;
            break;
        }
    }
    if (!matched) {
        return false;
    }
    unsigned month = 0;
    if (!one_or_two_digits(text, i, month)) {
        return false;
    }
    matched = false;
    for (const std::string_view sep : kDateSeps) {
        if (eat_sep(text, i, sep)) {
            matched = true;
            break;
        }
    }
    if (!matched) {
        return false;
    }
    unsigned day = 0;
    if (!one_or_two_digits(text, i, day)) {
        return false;
    }

    out.year = year;
    out.month = month;
    out.day = day;

    if (i == text.size()) {
        return true;  // 纯日期
    }

    // 日期→时间分隔：T / 空格 / 日。"日"后可无时间（中文日期结尾）。
    static constexpr std::string_view kTimeSeps[] = {"T", " ", "\xE6\x97\xA5"};
    matched = false;
    for (const std::string_view sep : kTimeSeps) {
        if (eat_sep(text, i, sep)) {
            matched = true;
            break;
        }
    }
    if (!matched) {
        return false;  // 日期后有杂质
    }
    if (i == text.size()) {
        return true;  // "…日" 结尾，无时间部分
    }

    unsigned hour = 0;
    if (!one_or_two_digits(text, i, hour)) {
        return false;
    }
    static constexpr std::string_view kHmSeps[] = {":", "\xE6\x97\xB6"};  // 时
    matched = false;
    for (const std::string_view sep : kHmSeps) {
        if (eat_sep(text, i, sep)) {
            matched = true;
            break;
        }
    }
    if (!matched) {
        return false;
    }
    if (i == text.size()) {
        return true;  // "…9时" 无分（中文口语形态）
    }
    unsigned minute = 0;
    if (!one_or_two_digits(text, i, minute)) {
        return false;
    }
    out.has_time = true;
    out.hour = hour;
    out.minute = minute;

    // 可选秒：":SS" / "分SS"（"…30分" 无秒也合法）。
    static constexpr std::string_view kMsSeps[] = {":", "\xE5\x88\x86"};  // 分
    matched = false;
    for (const std::string_view sep : kMsSeps) {
        if (eat_sep(text, i, sep)) {
            matched = true;
            break;
        }
    }
    if (matched) {
        if (i == text.size()) {
            return true;
        }
        unsigned second = 0;
        if (!one_or_two_digits(text, i, second)) {
            return false;
        }
        out.second = second;
    }

    // 可选小数秒（仅在秒之后）：".fff"。
    if (i < text.size() && text[i] == '.') {
        ++i;
        std::size_t digits = 0;
        while (i < text.size() && is_digit(text[i])) {
            ++i;
            ++digits;
        }
        if (digits == 0) {
            return false;
        }
    }

    // 可选时区：Z / z / ±HH[:MM] / ±HHMM / ±HH。
    if (i < text.size() && (text[i] == 'Z' || text[i] == 'z')) {
        ++i;
    } else if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
        const int sign = text[i] == '-' ? -1 : 1;
        ++i;
        unsigned off_h = 0;
        unsigned off_m = 0;
        if (!one_or_two_digits(text, i, off_h)) {
            return false;
        }
        if (i < text.size() && text[i] == ':') {
            ++i;
        }
        if (i < text.size() && is_digit(text[i])) {
            if (!one_or_two_digits(text, i, off_m)) {
                return false;
            }
        }
        out.tz_offset_minutes = sign * static_cast<int>(off_h * 60 + off_m);
    }

    return i == text.size();  // 尾部有杂质 → 不是时间形状
}

bool looks_like_timestamp(std::string_view text) noexcept {
    CivilParts parts;
    return split_timestamp_shape(text, parts);
}

const char* cardinality_tier_name(CardinalityTier tier) noexcept {
    switch (tier) {
    case CardinalityTier::Constant: return "constant";
    case CardinalityTier::Low:      return "low";
    case CardinalityTier::Mid:      return "mid";
    case CardinalityTier::High:     return "high";
    }
    return "?";
}

const char* column_kind_name(ColumnKind kind) noexcept {
    switch (kind) {
    case ColumnKind::Unknown:   return "unknown";
    case ColumnKind::NullOnly:  return "null_only";
    case ColumnKind::Timestamp: return "timestamp";
    case ColumnKind::Numeric:   return "numeric";
    case ColumnKind::Boolean:   return "boolean";
    case ColumnKind::Text:      return "text";
    }
    return "?";
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
