// series.hpp —— 数据系列（值类型，header-only）
//
// 顺带放**默认配色的唯一真源**：它在本文件而不是 chart.hpp，是因为 chart.hpp
// include 了本文件，反过来会成环。
#ifndef TERMCHART_CHART_SERIES_HPP
#define TERMCHART_CHART_SERIES_HPP

#include <string>
#include <vector>

#include "termchart/core/style.hpp"
#include "termchart/core/point.hpp"

namespace termchart::chart {

// 历史上每个图种各自硬编码 `Color::ansi16(7/2/1/4/8/6/3)`：同一件事七处写法，
// 将来配色表（palette）落地时要改得动，必须先收敛到一处。
// **色号本身没变**（逐个映射的结果与原来逐字相同），这一步只做"命名"。
enum class DefaultColor {
    Series,   // 7：中性前景，数据与系列默认
    Accent,   // 2：绿，正向 / 主强调
    Warn,     // 1：红，负向 / 告警
    Neutral,  // 4：蓝，统计类默认
    Muted,    // 8：暗灰，次要元素（轨道、连线）
    Cyan,     // 6：青，金融 / 极坐标类
    Amber,    // 3：黄，星标等点缀
};

inline Color default_color(DefaultColor which = DefaultColor::Series) noexcept {
    switch (which) {
        case DefaultColor::Accent:
            return Color::ansi16(2);
        case DefaultColor::Warn:
            return Color::ansi16(1);
        case DefaultColor::Neutral:
            return Color::ansi16(4);
        case DefaultColor::Muted:
            return Color::ansi16(8);
        case DefaultColor::Cyan:
            return Color::ansi16(6);
        case DefaultColor::Amber:
            return Color::ansi16(3);
        case DefaultColor::Series:
            break;
    }
    return Color::ansi16(7);
}

struct Series {
    std::string name;
    Color color = default_color(DefaultColor::Series);
    std::vector<Point> points;  // 数据坐标 (x, y)
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_SERIES_HPP
