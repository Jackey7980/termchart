#include "termchart/util/format.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <system_error>

#include "termchart/util/utf8.hpp"

namespace termchart::util {
namespace {

constexpr std::size_t kBufferSize = 64;

std::string_view fixed_to_chars(char* buffer, std::size_t capacity, double value, int precision) {
    const int clamped = precision < 0 ? 0 : (precision > 12 ? 12 : precision);
    const auto result = std::to_chars(buffer, buffer + capacity, value,
                                      std::chars_format::fixed, clamped);
    if (result.ec != std::errc{}) {
        return std::string_view{};
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

}  // namespace termchart::util
