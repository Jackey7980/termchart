// time_parse.cpp —— ISO-8601 形状时间 → epoch 秒
#include "termchart/autoviz/time_parse.hpp"

#include <array>
#include <cstdint>

namespace termchart::autoviz {
namespace {

bool is_digit(char ch) noexcept { return ch >= '0' && ch <= '9'; }

bool two_digits(std::string_view text, std::size_t at, int& out) noexcept {
    if (at + 1 >= text.size() || !is_digit(text[at]) || !is_digit(text[at + 1])) {
        return false;
    }
    out = (text[at] - '0') * 10 + (text[at + 1] - '0');
    return true;
}

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

// 严格日期校验（与只认形状的 looks_like_timestamp 的分工在此：非法日期拒绝）。
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
    if (text.size() < 10) {
        return std::nullopt;
    }
    std::int64_t year = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        if (!is_digit(text[i])) {
            return std::nullopt;
        }
        year = year * 10 + (text[i] - '0');
    }
    if (text[4] != '-' || text[7] != '-') {
        return std::nullopt;
    }
    int month = 0;
    int day = 0;
    if (!two_digits(text, 5, month) || !two_digits(text, 8, day)) {
        return std::nullopt;
    }
    if (!valid_date(year, static_cast<unsigned>(month), static_cast<unsigned>(day))) {
        return std::nullopt;
    }
    std::int64_t epoch = days_from_civil(year, static_cast<unsigned>(month),
                                         static_cast<unsigned>(day)) *
                         86400LL;

    if (text.size() == 10) {
        return static_cast<double>(epoch);
    }
    std::size_t i = 10;
    if (text[i] != ' ' && text[i] != 'T') {
        return std::nullopt;
    }
    ++i;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (!two_digits(text, i, hour)) {
        return std::nullopt;
    }
    i += 2;
    if (i >= text.size() || text[i] != ':') {
        return std::nullopt;
    }
    ++i;
    if (!two_digits(text, i, minute)) {
        return std::nullopt;
    }
    i += 2;
    if (i < text.size() && text[i] == ':') {
        ++i;
        if (!two_digits(text, i, second)) {
            return std::nullopt;
        }
        i += 2;
    }
    if (hour > 23 || minute > 59 || second > 60) {
        return std::nullopt;
    }
    epoch += hour * 3600 + minute * 60 + second;

    if (i < text.size() && text[i] == '.') {  // 毫秒（计入，不影响排序粒度）
        ++i;
        std::size_t digits = 0;
        double fraction = 0.0;
        double scale = 0.1;
        while (i < text.size() && is_digit(text[i])) {
            fraction += static_cast<double>(text[i] - '0') * scale;
            scale *= 0.1;
            ++i;
            ++digits;
        }
        if (digits == 0) {
            return std::nullopt;
        }
        epoch += static_cast<std::int64_t>(fraction);  // 只取整秒（可视化够用）
    }

    if (i < text.size()) {  // 时区后缀：Z 或 ±HH:MM / ±HHMM
        if (text[i] == 'Z') {
            ++i;
        } else if (text[i] == '+' || text[i] == '-') {
            const int sign = text[i] == '-' ? -1 : 1;
            ++i;
            int off_h = 0;
            int off_m = 0;
            if (!two_digits(text, i, off_h)) {
                return std::nullopt;
            }
            i += 2;
            if (i < text.size() && text[i] == ':') {
                ++i;
            }
            if (i < text.size()) {
                if (!two_digits(text, i, off_m)) {
                    return std::nullopt;
                }
                i += 2;
            }
            if (off_h > 23 || off_m > 59) {
                return std::nullopt;
            }
            epoch -= static_cast<std::int64_t>(sign) * (off_h * 3600 + off_m * 60);
        } else {
            return std::nullopt;
        }
    }
    if (i != text.size()) {
        return std::nullopt;
    }
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
