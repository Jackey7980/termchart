// data/scalar_parse.hpp —— 脏标量解析（"赛博法医"入口，L4）
//
// 真实世界的数值从来不是干净的 double：财报里是 "1,234.50"、UI 上是 "15%"、
// 会计系统导出 "(123.45)"、中日韩环境混用全角空格与 U+2212 负号、缺失是
// "NA"/"N/A"/"null"/"-"。io/csv 的数值入口遇到这些只能报错——本文件就是
// 在报错之前的那道法医层：**尽力从脏字符串里提取数值**，提不出就明确说
// 提不出（NotNumeric），是缺失记号就明确说是缺失（Empty/NaN）。
//
// 支持（可组合）：
//   * 前后空白（ASCII + 全角 U+3000）；
//   * 千分位逗号 "1,234.50"；
//   * 货币符号 "$" "¥" "€" "£" 与词 "RMB" "USD" "CNY"，中文后缀 "元"；
//   * 百分号 "15%" → 0.15；
//   * 会计括号 "(123.45)" → -123.45；
//   * Unicode 负号 U+2212 "−123"；
//   * 缺失记号（NA 族，大小写不敏感）→ NaN + Empty；
// 科学计数法由 std::from_chars 原生支持（"1e3"）。
#ifndef TERMCHART_DATA_SCALAR_PARSE_HPP
#define TERMCHART_DATA_SCALAR_PARSE_HPP

#include <cstdint>
#include <string_view>

namespace termchart::data {

enum class ScalarParseStatus : std::uint8_t {
    Ok,          // 解析出数值
    Empty,       // 缺失记号（NA 族或纯空白）→ value = NaN
    NotNumeric,  // 不是数值（提不出）→ value = NaN
};

struct ScalarParseResult {
    double value = 0.0;
    ScalarParseStatus status = ScalarParseStatus::Ok;

    [[nodiscard]] bool ok() const noexcept { return status == ScalarParseStatus::Ok; }
    [[nodiscard]] bool missing() const noexcept { return status == ScalarParseStatus::Empty; }
};

// 脏标量 → 数值。不抛异常；永不崩（任意字节序列都是合法输入）。
[[nodiscard]] ScalarParseResult parse_scalar(std::string_view text) noexcept;

// 是否为缺失记号（NA 族 / 纯空白）。
[[nodiscard]] bool is_missing_token(std::string_view text) noexcept;

}  // namespace termchart::data

#endif  // TERMCHART_DATA_SCALAR_PARSE_HPP
