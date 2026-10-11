// clipboard.cpp —— OSC 52 剪贴板实现
#include "termchart/term/clipboard.hpp"

#include <string>
#include <string_view>

#include "termchart/util/base64.hpp"

namespace termchart::term {
namespace {

constexpr std::string_view kOsc = "\x1b]";
constexpr std::string_view kSt = "\x1b\\";

std::string osc52(char target, std::string_view base64) {
    std::string out(kOsc);
    out += "52;";
    out += target;
    out += ';';
    out += base64;
    out += kSt;
    return out;
}

}  // namespace

std::string clipboard_set(std::string_view text, ClipboardTarget target) {
    return osc52(static_cast<char>(target), util::base64_encode(text));
}

std::string clipboard_request(ClipboardTarget target) {
    return osc52(static_cast<char>(target), {});
}

bool parse_clipboard_reply(std::string_view payload, std::string& out) {
    // 载荷形如 "<target>;<base64>"；target 只取一个字符，其后必须是 ';'。
    if (payload.size() < 2 || payload[1] != ';') {
        return false;
    }
    const std::string_view encoded = payload.substr(2);
    if (encoded.empty()) {
        return false;  // 终端拒绝读取，或剪贴板为空
    }
    return util::base64_decode(encoded, out);
}

}  // namespace termchart::term
