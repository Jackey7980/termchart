// hyperlink.cpp —— OSC 8 超链接实现
#include "termchart/term/hyperlink.hpp"

#include <string>
#include <string_view>

namespace termchart::term {
namespace {

// OSC 命令前缀 + 字符串终止符（ST）。
constexpr std::string_view kOsc = "\x1b]";
constexpr std::string_view kSt = "\x1b\\";

// 控制字符必须剔除：ESC 会截断序列，BEL/换行会提前结束或引发意外行为。
bool is_control(char ch) noexcept {
    return ch == '\x1b' || ch == '\x07' || ch == '\n' || ch == '\r' || ch == '\t';
}

// 参数位（id）：';' 是字段分隔符，必须一并剔除，否则字段边界会错位。
std::string sanitize_param(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char ch : text) {
        if (is_control(ch) || ch == ';') {
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

// URI 位（最后一个字段）：';' 在 URI 里是合法字符（矩阵参数等），不能删——
// 只剔除控制字符，保持链接原样。
std::string sanitize_uri(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char ch : text) {
        if (!is_control(ch)) {
            out.push_back(ch);
        }
    }
    return out;
}

}  // namespace

std::string hyperlink_open(std::string_view uri, std::string_view id) {
    std::string out(kOsc);
    out += '8';
    out += ';';
    if (!id.empty()) {
        out += "id=";
        out += sanitize_param(id);
        out += ';';
    } else {
        out += ';';
    }
    out += sanitize_uri(uri);
    out += kSt;
    return out;
}

std::string hyperlink_close() {
    std::string out(kOsc);
    out += "8;;";
    out += kSt;
    return out;
}

std::string hyperlink(std::string_view text, std::string_view uri, std::string_view id) {
    std::string out = hyperlink_open(uri, id);
    out += text;
    out += hyperlink_close();
    return out;
}

}  // namespace termchart::term
