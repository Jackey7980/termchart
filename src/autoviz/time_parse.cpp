// time_parse.cpp —— ISO 家族时间 → epoch 秒
//
// 形状扫描复用 data::split_timestamp_shape（全库唯一真源，避免两套格式族
// 漂移）；本文件只做它不做的事：**历法校验**（2026/13/40 形状合法但历法
// 非法，在这里拒绝）与 epoch 换算（含时区偏移）。
#include "termchart/autoviz/time_parse.hpp"

#include <array>
#include <cstdint>
#include <optional>

#include "termchart/data/column_profile.hpp"

namespace termchart::autoviz {
namespace {

// 民用日期 → 天数（1970-01-01 = 0）。Howard Hinnant 的 days_from_civil。
std::int64_t days_from_civil(std::int64_t y, unsigned m, unsigned d) noexcept {
    y -= m <= 2;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const auto yoe = static_cast<unsigned>(y - era * 400);          // [0, 399]
    const unsigned doy = (153U * (m + (m > 2 ? -3 : 9)) + 2U) / 5U + d - 1U;  // [0, 365]
    const unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;  // [0, 146096]
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

bool is_leap(std::int64_t y) noexcept {
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

// 严格日期校验（与只认形状的 split_timestamp_shape 的分工在此）。
bool valid_date(std::int64_t y, unsigned m, unsigned d) noexcept {
    if (y < 1400 || y > 9999 || m < 1 || m > 12 || d < 1) {
        return false;
    }
    static constexpr unsigned kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    unsigned limit = kDays[m - 1];
    if (m == 2 && is_leap(y)) {
        limit = 29;
    }
    return d <= limit;
}

// 1-2-5 阶梯（秒）。覆盖秒→年，够时间序列可视化的全部分桶粒度。
constexpr std::array<double, 21> kBucketLadder = {
    1.0,     2.0,     5.0,     10.0,     15.0,     30.0,     60.0,     120.0,    300.0,
    600.0,   900.0,   1800.0,  3600.0,   7200.0,   21600.0,  43200.0,  86400.0,   604800.0,
    2592000.0, 7776000.0, 31536000.0};

}  // namespace

std::optional<double> parse_timestamp(std::string_view text) noexcept {
    data::CivilParts parts;
    if (!data::split_timestamp_shape(text, parts)) {
        return std::nullopt;
    }
    if (!valid_date(parts.year, parts.month, parts.day)) {
        return std::nullopt;
    }
    if (parts.hour > 23 || parts.minute > 59 || parts.second > 60) {
        return std::nullopt;
    }
    constexpr int kMaxOffset = 23 * 60 + 59;  // 时区偏移 ±23:59 封顶
    if (parts.tz_offset_minutes < -kMaxOffset || parts.tz_offset_minutes > kMaxOffset) {
        return std::nullopt;
    }
    std::int64_t epoch = days_from_civil(parts.year, parts.month, parts.day) * 86400LL;
    epoch += static_cast<std::int64_t>(parts.hour) * 3600 +
             static_cast<std::int64_t>(parts.minute) * 60 + static_cast<std::int64_t>(parts.second);
    epoch -= static_cast<std::int64_t>(parts.tz_offset_minutes) * 60;  // 东区 = UTC 之东
    return static_cast<double>(epoch);
}

double nice_bucket_seconds(double t0, double t1, std::size_t target_buckets) noexcept {
    if (!(t1 > t0) || target_buckets == 0) {
        return 1.0;
    }
    const double want = (t1 - t0) / static_cast<double>(target_buckets);
    for (const double step : kBucketLadder) {
        if (step >= want) {
            return step;
        }
    }
    return kBucketLadder.back();
}

}  // namespace termchart::autoviz
