#include "termchart/util/format.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <system_error>

#include "termchart/util/utf8.hpp"

namespace termchart::util {
namespace {

constexpr std::size_t kBufferSize = 512;

std::string_view fixed_to_chars(char* buffer, std::size_t capacity, double value, int precision) {
    const int clamped = precision < 0 ? 0 : (precision > 12 ? 12 : precision);
    auto result = std::to_chars(buffer, buffer + capacity, value,
                                std::chars_format::fixed, clamped);
    if (result.ec != std::errc{}) {
        // 极端值（如 1e300 用 fixed+12 位小数会超过缓冲区 → value_too_large）退回
        // scientific，给出一个有界且可读的表示，而不是空白（见雷区1）。scientific
        // 对任意有限 double 都只有约 13 个字符，512 缓冲区永不溢出。
        result = std::to_chars(buffer, buffer + capacity, value,
                               std::chars_format::scientific, std::min(clamped, 6));
        if (result.ec != std::errc{}) {
            return std::string_view{};
        }
    }
    return std::string_view(buffer, static_cast<std::size_t>(result.ptr - buffer));
}

}  // namespace

std::string format_double(double value, int precision) {
    if (!std::isfinite(value)) {
        return std::isnan(value) ? std::string("nan")
                                 : (value > 0.0 ? std::string("inf") : std::string("-inf"));
    }
    char buffer[kBufferSize]{};
    const std::string_view text = fixed_to_chars(buffer, kBufferSize, value, precision);
    return text.empty() ? std::string() : std::string(text);
}

std::string format_double_trimmed(double value, int precision) {
    std::string out = format_double(value, precision);
    if (out.find('.') == std::string::npos || out == "nan" || out == "inf" || out == "-inf") {
        return out;
    }
    while (out.size() > 1 && out.back() == '0') {
        out.pop_back();
    }
    if (out.back() == '.') {
        out.pop_back();
    }
    return out;
}

void append_double(std::string& out, double value, int precision) {
    if (!std::isfinite(value)) {
        out.append(std::isnan(value) ? "nan" : (value > 0.0 ? "inf" : "-inf"));
        return;
    }
    char buffer[kBufferSize]{};
    const std::string_view text = fixed_to_chars(buffer, kBufferSize, value, precision);
    out.append(text);
}

std::string format_int(std::int64_t value) {
    char buffer[24]{};
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, result.ptr);
}

std::string format_compact(double value, int precision) {
    const double abs = std::fabs(value);
    if (!std::isfinite(value)) {
        return format_double(value, 0);
    }
    struct Unit {
        double scale;
        const char* suffix;
    };
    static constexpr Unit kUnits[] = {
        {1e12, "T"}, {1e9, "G"}, {1e6, "M"}, {1e3, "k"}, {1.0, ""},
    };
    for (const Unit& unit : kUnits) {
        if (abs >= unit.scale) {
            return format_double_trimmed(value / unit.scale, precision) + unit.suffix;
        }
    }
    return format_double_trimmed(value, precision);
}

std::string to_hex_string(const Rgb& color) {
    static constexpr char kDigits[] = "0123456789ABCDEF";
    // 预置长度直接按下标写，避免 reserve + 6 次 push_back 的容量检查。
    std::string out(7, '#');
    out[1] = kDigits[(color.r >> 4) & 0x0Fu];
    out[2] = kDigits[color.r & 0x0Fu];
    out[3] = kDigits[(color.g >> 4) & 0x0Fu];
    out[4] = kDigits[color.g & 0x0Fu];
    out[5] = kDigits[(color.b >> 4) & 0x0Fu];
    out[6] = kDigits[color.b & 0x0Fu];
    return out;
}

}  // namespace termchart::util
