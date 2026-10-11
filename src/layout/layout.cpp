// layout.cpp —— layout/ 公共工具 format_tick 的实现
#include "termchart/layout/layout.hpp"

#include <cstdio>

namespace termchart::layout {

std::string format_tick(double v) noexcept {
    if (v == 0.0) {
        return "0";
    }
    const double a = v < 0.0 ? -v : v;
    int prec = 2;
    if (a >= 100.0) {
        prec = 0;
    } else if (a >= 10.0) {
        prec = 1;
    } else if (a < 0.01) {
        prec = 1;
    }
    char buf[32];
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
