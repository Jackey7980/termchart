// series.hpp —— 数据系列（值类型，header-only）
#ifndef TERMCHART_CHART_SERIES_HPP
#define TERMCHART_CHART_SERIES_HPP

#include <string>
#include <vector>

#include "termchart/core/style.hpp"
#include "termchart/core/point.hpp"

namespace termchart::chart {

struct Series {
    std::string name;
    Color color = Color::ansi16(7);
    std::vector<Point> points;  // 数据坐标 (x, y)
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_SERIES_HPP
