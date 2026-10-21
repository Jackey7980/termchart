#ifndef TERMCHART_DATA_SCALAR_PARSE_HPP
#define TERMCHART_DATA_SCALAR_PARSE_HPP

// data/scalar_parse.hpp —— 脏标量清洗器（L4）
//
// "赛博法医"的第一块砖：把 io/csv、io/json 读出来的原始字符串，尽力解析成 double。
// 设计取向与 io/csv 的 field_to_double 相反——后者 fail-fast（干净数值表），本清洗器
// fail-safe：认不出就标 Invalid（保留原文给上层做分类维度），缺失记号标 Empty，
// 绝不抛异常、绝不崩。清洗动作顺手记成 ScalarFlag，并把货币/中文单位后缀抽到 unit。
//
// 覆盖：千分位逗号、货币符号（$¥€£ / RMB·USD·CNY）、百分号（15%→0.15）、
//       会计括号（(123.45)→-123.45）、Unicode 负号（U+2212）、全角数字/负号/小数点、
//       NA 族（NA/N/A/NaN/NULL/空/单个 - 等 → 缺失）。

#include <cstdint>
#include <cmath>
#include <string>
#include <string_view>

namespace termchart::data {

// 解析结果状态：Invalid=认不出（仍保留原文），Empty=缺失记号，Numeric=成功成数。
enum class ScalarStatus : std::uint8_t { Invalid, Empty, Numeric };

// 清洗过程中识别出的"脏"特征位。
enum class ScalarFlag : std::uint8_t {
    None = 0,
    HasComma = 1 << 0,            // "1,234.50"
    AccountingNegative = 1 << 1,  // "(123.45)"
    Percent = 1 << 2,             // "15%" → 0.15
    FullWidth = 1 << 3,           // 全角数字/负号/小数点
    Currency = 1 << 4,            // 货币前缀 $¥€£ / RMB·USD·CNY 或中文后缀 元/美元…
};

constexpr ScalarFlag operator|(ScalarFlag a, ScalarFlag b) noexcept {
    return static_cast<ScalarFlag>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
constexpr ScalarFlag operator&(ScalarFlag a, ScalarFlag b) noexcept {
    return static_cast<ScalarFlag>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}
constexpr bool has_flag(ScalarFlag f, ScalarFlag bit) noexcept {
    return (static_cast<std::uint8_t>(f) & static_cast<std::uint8_t>(bit)) != 0;
}

struct ParsedScalar {
    ScalarStatus status = ScalarStatus::Invalid;
    double value = std::nan("");  // Numeric 时为解析值；其余为 NaN
    std::string unit;             // 抽离出的单位/货币后缀（如 "元"/"美元"），正常为空
    ScalarFlag flags = ScalarFlag::None;
};

// 脏标量清洗。详见文件顶部说明。
[[nodiscard]] ParsedScalar parse_scalar(std::string_view raw);

// 缺失记号判定（空/NA/N/A/NaN/NULL/单个 - 等，大小写不敏感）。供上层在解析前快速分流。
[[nodiscard]] bool is_missing_token(std::string_view text) noexcept;

}  // namespace termchart::data

#endif  // TERMCHART_DATA_SCALAR_PARSE_HPP
