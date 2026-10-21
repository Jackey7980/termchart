// scalar_parse.cpp —— 脏标量解析实现
//
// 管道：去空白 → 缺失记号判定 → 会计括号负号 → 剥货币前缀 → Unicode/全角负号 →
//       剥中文单位后缀 → 去千分位逗号 → 剥百分号 → 全角数字归一 → from_chars。
// 每一步都宽松（宁可多认，不可误判数值），最终认不出就是 Invalid——fail-loud 的是
// "结果状态"，不是"输入"。
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

// 全角空格 U+3000、Unicode 负号 U+2212 的 UTF-8 序列。
constexpr std::string_view kIdeographicSpace = "\xE3\x80\x80";
constexpr std::string_view kUnicodeMinus = "\xE2\x88\x92";

// 缺失记号（空串与单个 - 也在此列，大小写不敏感比较）。
constexpr std::array<std::string_view, 9> kMissingTokens = {
    "", "NA", "N/A", "NAN", "NULL", "NONE", "-", "--", "\xE2\x80\x94" /* em dash */};

// 前缀货币/单位词（剥到不能再剥，如 "$RMB100"）。
constexpr std::array<std::string_view, 7> kCurrencyPrefixes = {"$", "\xC2\xA5" /* ¥ */,
                                                               "\xE2\x82\xAC" /* € */,
                                                               "\xC2\xA3" /* £ */,
                                                               "RMB", "USD", "CNY"};

// 后缀货币/单位词（中文为主，抽到 unit 字段）。注意顺序：**多字后缀必须排在单字
// 之前**，否则 "100美元" 会先匹配到 "元" 留下 "100美" 而解析失败。
constexpr std::array<std::string_view, 8> kCurrencySuffixes = {
    "\xe7\xbe\x8e\xe5\x85\x83" /* 美元 */, "\xe6\xac\xa7\xe5\x85\x83" /* 欧元 */,
    "\xe8\x8b\xb1\xe9\x95\x91" /* 英镑 */, "\xe6\x97\xa5\xe5\x9c\x86" /* 日圆 */,
    "\xe6\xb8\xaf\xe5\xb8\x81" /* 港币 */, "\xe5\x85\x83" /* 元 */,
    "\xe5\x9c\x86" /* 圆 */, "\xe5\x9d\x97" /* 块 */};

std::string to_lower(std::string_view text) {
    std::string out(text);
    for (char& ch : out) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return out;
}

bool is_ascii_space(char ch) noexcept {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

std::string_view trim(std::string_view text) noexcept {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && is_ascii_space(text[begin])) ++begin;
    while (end > begin && is_ascii_space(text[end - 1])) --end;
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

// 解码一个 UTF-8 码点；返回码点与消耗的字节数（lead 非法时返回 {0,1} 跳过）。
std::pair<unsigned, std::size_t> decode_cp(std::string_view s, std::size_t i) noexcept {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) return {c, 1};
    unsigned cp;
    std::size_t len;
    if ((c & 0xE0) == 0xC0) {
        cp = c & 0x1F;
        len = 2;
    } else if ((c & 0xF0) == 0xE0) {
        cp = c & 0x0F;
        len = 3;
    } else if ((c & 0xF8) == 0xF0) {
        cp = c & 0x07;
        len = 4;
    } else {
        return {0, 1};
    }
    if (i + len > s.size()) return {0, 1};
    for (std::size_t k = 1; k < len; ++k) {
        const unsigned char d = static_cast<unsigned char>(s[i + k]);
        if ((d & 0xC0) != 0x80) return {0, 1};
        cp = (cp << 6) | (d & 0x3F);
    }
    return {cp, len};
}

// 全角 → 半角归一；遇任何全角数字/负号/小数点置 fullwidth=true。全角数字 U+FF10–FF19、
// 全角负号 U+FF0D、全角句点 U+FF0E、全角空格 U+3000；其余码点原样透传。
std::string normalize_fullwidth(std::string_view s, bool& fullwidth) {
    std::string out;
    out.reserve(s.size());
    fullwidth = false;
    std::size_t i = 0;
    while (i < s.size()) {
        const auto [cp, len] = decode_cp(s, i);
        char32_t ch = static_cast<char32_t>(cp);
        if (ch >= 0xFF10 && ch <= 0xFF19) {
            out.push_back(static_cast<char>('0' + (ch - 0xFF10)));
            fullwidth = true;
        } else if (ch == 0xFF0D) {  // 全角负号 －
            out.push_back('-');
            fullwidth = true;
        } else if (ch == 0xFF0E) {  // 全角句点 ．
            out.push_back('.');
            fullwidth = true;
        } else if (ch == 0x3000) {  // 全角空格
            out.push_back(' ');
            fullwidth = true;
        } else {
            out.append(s.substr(i, len));
        }
        i += len;
    }
    return out;
}

bool from_chars_strict(std::string_view text, double& out) noexcept {
    if (text.empty()) return false;
    std::string buf;
    std::string_view body = text;
    if (text.front() == '+') {  // from_chars 拒前导 '+'；"+5" 容忍掉
        buf.assign(text.substr(1));
        body = buf;
    }
    const char* first = body.data();
    const char* last = body.data() + body.size();
    double value = 0.0;
    const auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc{} || result.ptr != last) return false;  // 尾部有杂质
    if (!std::isfinite(value)) return false;                          // inf/nan 字面量
    out = value;
    return true;
}

}  // namespace

bool is_missing_token(std::string_view text) noexcept {
    const std::string_view t = trim(text);
    for (const std::string_view token : kMissingTokens) {
        if (t == token) return true;
    }
    const std::string low = to_lower(t);
    return low == "na" || low == "n/a" || low == "nan" || low == "null" || low == "none" ||
           t == "-" || t == "--";
}

ParsedScalar parse_scalar(std::string_view text) {
    ParsedScalar result;
    std::string_view t = trim(text);

    // 1) 缺失记号 → Empty（与 io/csv 的 NA 族口径一致）。
    if (is_missing_token(t)) {
        result.status = ScalarStatus::Empty;
        return result;
    }
    if (t.empty()) {
        result.status = ScalarStatus::Empty;
        return result;
    }

    // 2) 会计括号负号 "(123.45)"（先于货币剥离："( $1,000 )" 剥括号后才露出 $）。
    bool negative = false;
    if (t.size() >= 2 && t.front() == '(' && t.back() == ')') {
        t = t.substr(1, t.size() - 2);
        negative = true;
        t = trim(t);
        result.flags = result.flags | ScalarFlag::AccountingNegative;
    }

    // 3) 剥前缀货币/单位词（可能多个，如 "$RMB100"、"($USD 5)" 剥括号后露出的 $）。
    for (;;) {
        bool changed = false;
        for (const std::string_view prefix : kCurrencyPrefixes) {
            if (starts_with_ci(t, prefix)) {
                t.remove_prefix(prefix.size());
                result.flags = result.flags | ScalarFlag::Currency;
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
            if (t.front() == '-') negative = !negative;
            t.remove_prefix(1);
        }
    }

    // 5) 中文后缀（元/圆/块/美元…）：剥掉并记到 unit，同时置 Currency 位。
    for (const std::string_view suffix : kCurrencySuffixes) {
        if (ends_with_ci(t, suffix)) {
            result.unit.assign(suffix);
            t.remove_suffix(suffix.size());
            result.flags = result.flags | ScalarFlag::Currency;
            t = trim(t);
            break;
        }
    }

    // 6) 千分位逗号：直接去掉（宽松，不做分组位数校验）。
    std::string cleaned;
    cleaned.reserve(t.size());
    for (const char ch : t) {
        if (ch != ',') cleaned.push_back(ch);
    }
    if (t.find(',') != std::string_view::npos) {
        result.flags = result.flags | ScalarFlag::HasComma;
    }

    // 7) 百分号：除 100（"15%" / "15 %" 都认）。
    bool percent = false;
    if (!cleaned.empty() && cleaned.back() == '%') {
        cleaned.pop_back();
        percent = true;
        result.flags = result.flags | ScalarFlag::Percent;
    }
    while (!cleaned.empty() && is_ascii_space(cleaned.back())) cleaned.pop_back();

    // 8) 全角数字/负号/小数点归一（"１２３．５" → "123.5"）。
    bool fullwidth = false;
    std::string normalized = normalize_fullwidth(cleaned, fullwidth);
    if (fullwidth) result.flags = result.flags | ScalarFlag::FullWidth;

    // 9) 严格 from_chars。
    double value = 0.0;
    if (!from_chars_strict(normalized, value)) {
        result.status = ScalarStatus::Invalid;
        return result;
    }
    if (negative) value = -value;
    if (percent) value /= 100.0;
    result.value = value;
    result.status = ScalarStatus::Numeric;
    return result;
}

}  // namespace termchart::data
