// scalar_parse.cpp —— 脏标量解析实现
//
// 管道：去空白 → 缺失记号判定 → 剥货币 → 括号负号 → Unicode 负号 → 去千分位
//       → 剥百分号 → from_chars。每一步都宽松（宁可多认，不可误判数值），
//       最终认不出就是 NotNumeric——fail-loud 的是"结果"，不是"输入"。
#include "termchart/data/scalar_parse.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>

namespace termchart::data {
namespace {

// 全角空格 U+3000 的 UTF-8 序列（中日韩数据源常见）。
constexpr std::string_view kIdeographicSpace = "\xE3\x80\x80";
// Unicode 负号 U+2212 的 UTF-8 序列。
constexpr std::string_view kUnicodeMinus = "\xE2\x88\x92";

// 缺失记号（大小写不敏感比较）。
constexpr std::array<std::string_view, 9> kMissingTokens = {
    "", "NA", "N/A", "NAN", "NULL", "NONE", "-", "--", "\xE2\x80\x94" /* em dash */};

std::string to_lower(std::string_view text) {
    std::string out(text);
    for (char& ch : out) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return out;
}

bool is_ascii_space(char ch) noexcept {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' ||
           ch == '\v';
}

std::string_view trim(std::string_view text) noexcept {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && is_ascii_space(text[begin])) ++begin;
    while (end > begin && is_ascii_space(text[end - 1])) --end;
    // 全角空格（按序列剥，可能连续多个）。
    for (;;) {
        if (end - begin >= kIdeographicSpace.size() &&
            text.compare(begin, kIdeographicSpace.size(), kIdeographicSpace) == 0) {
            begin += static_cast<std::size_t>(kIdeographicSpace.size());
            continue;
        }
        if (end - begin >= kIdeographicSpace.size() &&
            text.compare(end - kIdeographicSpace.size(), kIdeographicSpace.size(),
                         kIdeographicSpace) == 0) {
            end -= static_cast<std::size_t>(kIdeographicSpace.size());
            continue;
        }
        break;
    }
    return text.substr(begin, end - begin);
}

bool starts_with_ci(std::string_view text, std::string_view prefix) noexcept {
    if (text.size() < prefix.size()) return false;
    return to_lower(text.substr(0, prefix.size())) == to_lower(prefix);
}

bool ends_with_ci(std::string_view text, std::string_view suffix) noexcept {
    if (text.size() < suffix.size()) return false;
    return to_lower(text.substr(text.size() - suffix.size())) == to_lower(suffix);
}

// 前缀货币/单位词（剥一个即可）。
constexpr std::array<std::string_view, 7> kCurrencyPrefixes = {"$", "\xC2\xA5" /* ¥ */,
                                                               "\xE2\x82\xAC" /* € */,
                                                               "\xC2\xA3" /* £ */,
                                                               "RMB", "USD", "CNY"};

bool from_chars_strict(std::string_view text, double& out) noexcept {
    if (text.empty()) return false;
    // from_chars 不接受前导 '+'；Excel/数据库偶见 "+5"， tolerated 掉。
    std::string buf;
    std::string_view body = text;
    if (text.front() == '+') {
        buf.assign(text.substr(1));
        body = buf;
    }
    const char* first = body.data();
    const char* last = body.data() + body.size();
    double value = 0.0;
    const auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc{} || result.ptr != last) {
        return false;  // 未耗尽 = 尾部有杂质（严格）
    }
    if (!std::isfinite(value)) {
        return false;  // inf/nan 字面量不当数值
    }
    out = value;
    return true;
}

}  // namespace

bool is_missing_token(std::string_view text) noexcept {
    const std::string_view t = trim(text);
    for (const std::string_view token : kMissingTokens) {
        if (t == token) return true;
    }
    return to_lower(t) == "na" || to_lower(t) == "n/a" || to_lower(t) == "nan" ||
           to_lower(t) == "null" || to_lower(t) == "none" || t == "-" || t == "--";
}

ScalarParseResult parse_scalar(std::string_view text) noexcept {
    ScalarParseResult result;
    std::string_view t = trim(text);

    // 1) 缺失记号 → NaN + Empty（与 io/csv 的 NA 族口径一致）。
    if (is_missing_token(t)) {
        result.value = std::nan("");
        result.status = ScalarParseStatus::Empty;
        return result;
    }
    if (t.empty()) {
        result.value = std::nan("");
        result.status = ScalarParseStatus::Empty;
        return result;
    }

    // 2) 会计括号负号 "(123.45)"（先于货币剥离："( $1,000 )" 剥括号后才露出 $）。
    bool negative = false;
    if (t.size() >= 2 && t.front() == '(' && t.back() == ')') {
        t = t.substr(1, t.size() - 2);
        negative = true;
        t = trim(t);
    }

    // 3) 剥前缀货币/单位词（可能多个，如 "$RMB100"、"($USD 5)" 剥括号后露出的 $ 也在此处理）。
    for (;;) {
        bool changed = false;
        for (const std::string_view prefix : kCurrencyPrefixes) {
            if (starts_with_ci(t, prefix)) {
                t.remove_prefix(prefix.size());
                changed = true;
                break;
            }
        }
        if (!changed) break;
    }
    t = trim(t);  // 货币与单位词之间可能留白（"USD 50"）

    // 4) Unicode 负号 → ASCII '-'（可能在括号内，也可能裸用）。
    if (t.size() >= kUnicodeMinus.size() &&
        t.compare(0, kUnicodeMinus.size(), kUnicodeMinus) == 0) {
        t.remove_prefix(kUnicodeMinus.size());
        negative = !negative;  // 与括号负号叠加时两次取反
        if (!t.empty() && (t.front() == '$' || t.front() == '-')) {
            // "−$5" / "−-5"：把随后的符号让位给 negative 逻辑。
            if (t.front() == '-') {
                negative = !negative;
            }
            t.remove_prefix(1);
        }
    }

    // 5) 中文后缀 "元"。
    if (ends_with_ci(t, "\xe5\x85\x83" /* 元 */)) {
        t.remove_suffix(3);
    }

    // 6) 千分位逗号：直接去掉（宽松，不做分组位数校验）。
    std::string cleaned;
    cleaned.reserve(t.size());
    for (const char ch : t) {
        if (ch != ',') cleaned.push_back(ch);
    }

    // 7) 百分号：除 100。
    bool percent = false;
    if (!cleaned.empty() && cleaned.back() == '%') {
        cleaned.pop_back();
        percent = true;
    }
    // 剥百分号后可能又剩空白（"15 %"）。
    while (!cleaned.empty() && is_ascii_space(cleaned.back())) cleaned.pop_back();

    // 8) 严格 from_chars。
    double value = 0.0;
    if (!from_chars_strict(cleaned, value)) {
        result.value = std::nan("");
        result.status = ScalarParseStatus::NotNumeric;
        return result;
    }
    if (negative) value = -value;
    if (percent) value /= 100.0;
    result.value = value;
    result.status = ScalarParseStatus::Ok;
    return result;
}

}  // namespace termchart::data
