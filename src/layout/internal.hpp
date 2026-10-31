// internal.hpp —— layout/ 各 .cpp 共享的字符常量与对齐工具（非公开，仅 src 内）
#pragma once

#include <cstddef>
#include <string>

#include "termchart/core/rect.hpp"
#include "termchart/layout/layout.hpp"
#include "termchart/layout/sink.hpp"

namespace termchart::layout {

// 占位级线/网格/刻度字符（最终观感由渲染层决定，这里只负责几何）。
// 线字符的**唯一真源**已上移到公开头 layout/sink.hpp：CanvasSink 与 SceneSink
// 都要用同一套字形，两处各写一份字面量迟早会分叉。这里保留旧名做别名。
inline constexpr char32_t kHLine = kSolidH;      // ─
inline constexpr char32_t kVLine = kSolidV;      // │
inline constexpr char32_t kTickDown = U'\u2534';   // ┴
inline constexpr char32_t kTickRight = U'\u2524';  // ┤
inline constexpr char32_t kTickLeft = U'\u251c';   // ├
inline constexpr char32_t kCorner = U'\u2514';   // └
inline constexpr char32_t kGridH = kDashedH;     // ┄
inline constexpr char32_t kGridV = kDashedV;     // ┆

// 把 text 按 halign 放到 [x0, x1] 区间内的起始列；超长则左对齐截断。
// len 为字符数（离散）：调用方负责把小数宽度取整（见 legend.cpp 的 std::round），
// 这里保持整数语义，避免把 -Wconversion 扩散到所有 .size() 调用点。
inline double align_start(double x0, double x1, std::size_t len, HAlign align) noexcept {
    const double span = x1 - x0 + 1.0;
    if (static_cast<double>(len) >= span) {
        return x0;
    }
    switch (align) {
        case HAlign::Left:
            return x0;
        case HAlign::Center:
            return x0 + (span - static_cast<double>(len)) / 2.0;
        case HAlign::Right:
            return x1 + 1.0 - static_cast<double>(len);
    }
    return x0;
}

}  // namespace termchart::layout
