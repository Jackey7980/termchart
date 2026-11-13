// series.hpp —— 数据系列（值类型，header-only）
//
// 顺带放**默认配色的唯一真源**：它在本文件而不是 chart.hpp，是因为 chart.hpp
// include 了本文件，反过来会成环。
#ifndef TERMCHART_CHART_SERIES_HPP
#define TERMCHART_CHART_SERIES_HPP

#include <string>
#include <vector>

#include "termchart/color/theme_palette.hpp"
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

// ── 全局默认调色板（可安装；缺省 = 保持历史 ansi16 默认色）──────────────
// 图表的默认色全部经 default_color() 这**一个**收敛点解析，所以"全局换肤"只需
// 装一张色板，不必逐个图表改参数。
//
// 为什么是进程级指针（与 widget 侧"按值传色板、无全局状态"不同）：图表的默认色
// 在**构造期**就要定下来，那时手上没有"当前色板"可传；而 default_color() 是
// 无参的默认参数求值点（`Color color = default_color(...)`），只能走全局。
// 缺省 nullptr = 不覆盖 = 逐字保持历史 ansi16 行为（金标快照零变化）。
inline const color::ThemePalette*& default_palette_slot() noexcept {
    static const color::ThemePalette* palette = nullptr;
    return palette;
}

// 安装全局默认调色板；传 nullptr 即回到历史 ansi16 默认色。
inline void set_default_palette(const color::ThemePalette* palette) noexcept {
    default_palette_slot() = palette;
}

// 当前生效的全局默认调色板（nullptr = 未安装，走历史 ansi16）。
[[nodiscard]] inline const color::ThemePalette* default_palette() noexcept {
    return default_palette_slot();
}

inline Color default_color(DefaultColor which = DefaultColor::Series) noexcept {
    // 装了色板：语义槽位映射到色板槽位（series/neutral/info/caution 与控件槽位
    // 一一对应，见 color::ThemePalette）。
    if (const color::ThemePalette* palette = default_palette_slot()) {
        switch (which) {
            case DefaultColor::Accent:
                return Color::rgb(palette->accent);
            case DefaultColor::Warn:
                return Color::rgb(palette->danger);
            case DefaultColor::Neutral:
                return Color::rgb(palette->neutral);
            case DefaultColor::Muted:
                return Color::rgb(palette->text_dim);
            case DefaultColor::Cyan:
                return Color::rgb(palette->info);
            case DefaultColor::Amber:
                return Color::rgb(palette->caution);
            case DefaultColor::Series:
                break;
        }
        return Color::rgb(palette->series);
    }
    // 未装色板：历史 ansi16 色号（逐字不变）。
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

    // 移动语义：把外部已构造好的点集整段搬入，避免一次 vector 拷贝
    // （Python / 外部桥批量灌数据时尤其有用）。
    void set_points(std::vector<Point>&& pts) noexcept { points = std::move(pts); }
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_SERIES_HPP
