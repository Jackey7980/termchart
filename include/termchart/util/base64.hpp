// base64.hpp —— 标准 base64（RFC 4648，含 '=' 填充）
//
// 本文件是**规格文件清单之外的补充**，理由：OSC 52 剪贴板（term/，L2）与
// kitty / iterm2 图像协议（protocol/，L9）都需要 base64，而 term 在 protocol 之下，
// 不能反向依赖。把唯一实现放在最底层 util/，所有需要方都调这里，避免出现第二份
// 实现后各自漂移（protocol 里原先那份内联实现已改调本文件）。
//
// 纯函数、无状态、无 I/O；列入 header-only 清单。
#ifndef TERMCHART_UTIL_BASE64_HPP
#define TERMCHART_UTIL_BASE64_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace termchart::util {

// 编码：标准字母表，长度不是 3 的倍数时补 '='。空输入返回空串。
[[nodiscard]] inline std::string base64_encode(const std::uint8_t* data, std::size_t len) {
    static constexpr char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    std::size_t i = 0;
    for (; i + 3 <= len; i += 3) {
        const std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                                static_cast<std::uint32_t>(data[i + 2]);
        out += kTable[(v >> 18) & 0x3F];
        out += kTable[(v >> 12) & 0x3F];
        out += kTable[(v >> 6) & 0x3F];
        out += kTable[v & 0x3F];
    }
    const std::size_t rest = len - i;
    if (rest == 1) {
        const std::uint32_t v = static_cast<std::uint32_t>(data[i]) << 16;
        out += kTable[(v >> 18) & 0x3F];
        out += kTable[(v >> 12) & 0x3F];
        out += "==";
    } else if (rest == 2) {
        const std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8);
        out += kTable[(v >> 18) & 0x3F];
        out += kTable[(v >> 12) & 0x3F];
        out += kTable[(v >> 6) & 0x3F];
        out += '=';
    }
    return out;
}

[[nodiscard]] inline std::string base64_encode(std::string_view text) {
    return base64_encode(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}

// 解码：忽略空白（终端回传的 OSC 52 常带换行），遇到 '=' 即结束。
// 字母表之外的字符视为非法，返回 false 并保持 out 内容不变。
[[nodiscard]] inline bool base64_decode(std::string_view text, std::string& out) {
    auto value_of = [](char ch) -> int {
        if (ch >= 'A' && ch <= 'Z') {
            return ch - 'A';
        }
        if (ch >= 'a' && ch <= 'z') {
            return ch - 'a' + 26;
        }
        if (ch >= '0' && ch <= '9') {
            return ch - '0' + 52;
        }
        if (ch == '+') {
            return 62;
        }
        if (ch == '/') {
            return 63;
        }
        return -1;
    };

    std::string decoded;
    decoded.reserve(text.size() / 4 * 3);
    std::uint32_t accum = 0;
    int bits = 0;
    for (const char ch : text) {
        if (ch == '=') {
            break;  // 填充只出现在末尾
        }
        if (ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t') {
            continue;
        }
        const int value = value_of(ch);
        if (value < 0) {
            return false;
        }
        accum = (accum << 6) | static_cast<std::uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            decoded.push_back(static_cast<char>((accum >> bits) & 0xFFu));
        }
    }
    out = std::move(decoded);
    return true;
}

}  // namespace termchart::util

#endif  // TERMCHART_UTIL_BASE64_HPP
