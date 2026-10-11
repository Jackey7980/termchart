#include "termchart/util/utf8.hpp"

namespace termchart::util {
namespace {

// 期望的续字节数：0xFF 表示非法首字节。
int trailing_count(unsigned char lead) noexcept {
    if (lead < 0x80u) {
        return 0;
    }
    if (lead >= 0xC2u && lead <= 0xDFu) {
        return 1;
    }
    if (lead >= 0xE0u && lead <= 0xEFu) {
        return 2;
    }
    if (lead >= 0xF0u && lead <= 0xF4u) {
        return 3;
    }
    return -1;  // 0x80..0xC1 与 0xF5..0xFF：非法首字节
}

}  // namespace

Utf8Decode decode(std::string_view bytes, std::size_t pos) noexcept {
    Utf8Decode out;
    if (pos >= bytes.size()) {
        return out;
    }
    const unsigned char lead = static_cast<unsigned char>(bytes[pos]);
    const int trailing = trailing_count(lead);
    if (trailing < 0) {
        out.cp = kReplacementChar;
        out.consumed = 1;
        return out;
    }
    const std::size_t available = bytes.size() - pos - 1;
    const std::size_t need = static_cast<std::size_t>(trailing);
    if (available < need) {
        out.cp = kReplacementChar;
        out.consumed = static_cast<int>(available + 1);
        out.incomplete = true;
        return out;
    }

    // 首字节有效位。
    std::uint32_t cp = 0;
    int shift_bits = 0;
    switch (trailing) {
        case 0:
            cp = lead;
            break;
        case 1:
            cp = static_cast<std::uint32_t>(lead & 0x1Fu);
            shift_bits = 6;
            break;
        case 2:
            cp = static_cast<std::uint32_t>(lead & 0x0Fu);
            shift_bits = 12;
            break;
        default:
            cp = static_cast<std::uint32_t>(lead & 0x07u);
            shift_bits = 18;
            break;
    }

    for (int i = 0; i < trailing; ++i) {
        const unsigned char cont = static_cast<unsigned char>(bytes[pos + 1 + static_cast<std::size_t>(i)]);
        if ((cont & 0xC0u) != 0x80u) {
            out.cp = kReplacementChar;
            out.consumed = i + 1;  // 停在这个非续字节之前
            return out;
        }
        cp = (cp << 6) | static_cast<std::uint32_t>(cont & 0x3Fu);
        shift_bits -= 6;
    }

    // 超长编码 / 代理区 / 越界：一律 U+FFFD，但仍消耗完整序列。
    const bool overlong = (trailing == 1 && cp < 0x80u) || (trailing == 2 && cp < 0x800u) ||
                          (trailing == 3 && cp < 0x10000u);
    const bool surrogate = cp >= 0xD800u && cp <= 0xDFFFu;
    if (overlong || surrogate || cp > 0x10FFFFu) {
        out.cp = kReplacementChar;
        out.consumed = trailing + 1;
        return out;
    }

    out.cp = static_cast<char32_t>(cp);
    out.consumed = trailing + 1;
    out.ok = true;
    return out;
}

std::size_t encode(char32_t cp, char* out) noexcept {
    const std::uint32_t value = static_cast<std::uint32_t>(cp);
    if (value <= 0x7Fu) {
        out[0] = static_cast<char>(value);
        return 1;
    }
    if (value <= 0x7FFu) {
        out[0] = static_cast<char>(0xC0u | (value >> 6));
        out[1] = static_cast<char>(0x80u | (value & 0x3Fu));
        return 2;
    }
    if (value <= 0xFFFFu) {
        if (value >= 0xD800u && value <= 0xDFFFu) {
            return 0;  // 代理码点不可编码进 UTF-8
        }
        out[0] = static_cast<char>(0xE0u | (value >> 12));
        out[1] = static_cast<char>(0x80u | ((value >> 6) & 0x3Fu));
        out[2] = static_cast<char>(0x80u | (value & 0x3Fu));
        return 3;
    }
    if (value <= 0x10FFFFu) {
        out[0] = static_cast<char>(0xF0u | (value >> 18));
        out[1] = static_cast<char>(0x80u | ((value >> 12) & 0x3Fu));
        out[2] = static_cast<char>(0x80u | ((value >> 6) & 0x3Fu));
        out[3] = static_cast<char>(0x80u | (value & 0x3Fu));
        return 4;
    }
    return 0;
}

void append(std::string& out, char32_t cp) {
    char buf[kMaxUtf8Bytes]{};
    const std::size_t n = encode(cp, buf);
    if (n != 0) {
        out.append(buf, n);
        return;
    }
    // 不可编码：按替换字符落地，保证调用方拿到的仍是合法 UTF-8。
    char replacement[kMaxUtf8Bytes]{};
    const std::size_t rn = encode(kReplacementChar, replacement);
    out.append(replacement, rn);
}

std::string to_utf8(char32_t cp) {
    std::string out;
    append(out, cp);
    return out;
}

std::size_t codepoint_count(std::string_view text) noexcept {
    std::size_t count = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const Utf8Decode d = decode(text, pos);
        if (d.consumed == 0) {
            break;
        }
        pos += static_cast<std::size_t>(d.consumed);
        ++count;
    }
    return count;
}

bool is_valid(std::string_view text) noexcept {
    std::size_t pos = 0;
    while (pos < text.size()) {
        const Utf8Decode d = decode(text, pos);
        if (!d.ok || d.consumed == 0) {
            return false;
        }
        pos += static_cast<std::size_t>(d.consumed);
    }
    return true;
}

Result<std::string> sanitize(std::string_view text) noexcept {
    std::string out;
    out.reserve(text.size());
    std::size_t pos = 0;
    while (pos < text.size()) {
        const Utf8Decode d = decode(text, pos);
        if (d.consumed == 0) {
            break;
        }
        if (d.ok) {
            out.append(text.data() + pos, static_cast<std::size_t>(d.consumed));
        } else {
            append(out, kReplacementChar);
        }
        pos += static_cast<std::size_t>(d.consumed);
    }
    return out;
}

}  // namespace termchart::util
