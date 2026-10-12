// metro_map.hpp —— 地铁线路图（线路 = 有序站点序列 + 线色）
//
// 字符化策略：站点画带边框的小盒并居中标名（同名站点合并为换乘站），线路
// 沿 45° 走线并按线色着色（复用 layout_metro + route_metro，不画箭头）。
#ifndef TERMCHART_GRAPH_DIAGRAMS_METRO_MAP_HPP
#define TERMCHART_GRAPH_DIAGRAMS_METRO_MAP_HPP

#include <string>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/layout/metro.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct MetroLine {
    std::string name;                   // 线路名（预留，当前不绘制）
    Color color{};                      // 线色；none 时用 fallback_line_style
    std::vector<std::string> stations;  // 有序站点（同名 = 换乘站）
};

struct MetroMapOptions {
    MetroOptions metro{};
    Style station_style{};
    Style fallback_line_style{};  // 线路未指定颜色时的走线样式
    bool station_borders = true;
};

// 就地布局并绘制地铁线路图，返回整图包围盒。空站点序列的线路被忽略。
TERMCHART_API Rect render_metro_map(const std::vector<MetroLine>& lines,
                                    canvas::Canvas& canvas,
                                    const MetroMapOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_METRO_MAP_HPP
