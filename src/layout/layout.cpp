// layout.cpp —— layout/ 公共工具 format_tick 的实现
#include "termchart/layout/layout.hpp"

#include <cmath>
#include <cstdio>

namespace termchart::layout {
namespace {

// 紧凑科学计数：%g 在指数 >= prec 时自动切到 e 记法，输出长度可控。
// 上限口径：默认左边距 8 列，标签可用 7 列（"1.2e+20" 恰好 7 列）。
bool needs_scientific(double a) noexcept { return a >= 1e7 || a < 1e-3; }

// 超宽时退一位有效数字：负数多一个 '-'，"-1.2e+09" 是 8 列、"-1e+09" 才 6 列。
std::string format_scientific(double v) noexcept {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2g", v);
    std::string s(buf);
    if (s.size() > 7) {
        std::snprintf(buf, sizeof(buf), "%.1g", v);
        s = buf;
    }
    return s;
}

}  // namespace

std::string format_tick(double v) noexcept {
    if (v == 0.0) {  // 同时吃掉 -0.0
        return "0";
    }
    if (!std::isfinite(v)) {
        return v < 0.0 ? "-inf" : "inf";
    }
    const double a = v < 0.0 ? -v : v;
    char buf[32];
    if (needs_scientific(a)) {
        return format_scientific(v);
    }
    int prec = 2;
    if (a >= 100.0) {
        prec = 0;
    } else if (a >= 10.0) {
        prec = 1;
    } else if (a < 0.01) {
        prec = 1;
    }
    std::snprintf(buf, sizeof(buf), "%.*f", prec, v);
    // 去掉尾随的 '0' 与可能的 '.'（"1.0" → "1"，"0.50" → "0.5"）。
    std::string s(buf);
    if (s.find('.') != std::string::npos) {
        while (s.size() > 1 && s.back() == '0') {
            s.pop_back();
        }
        if (s.back() == '.') {
            s.pop_back();
        }
    }
    return s;
}

}  // namespace termchart::layout
