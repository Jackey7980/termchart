// 19_metro_map.cpp —— 地铁线路图（示意式：环线 + 两条直径 + 交路）
//
// 这是"交汇口规则抽成公共件"之后的第一个消费者示例，也是**刻意不复制规则**的示范：
//   * 线段字符（─ │ ╱ ╲）由方向决定；
//   * 环线四角的 ┌ ┐ └ ┘ 直接来自 `graph/route/junction` 的退化口径；
//   * 站名的避让**批量交给 `layout::place_labels`**（线路格与车站格作为占用集，
//     成功的标签自动进占用集）——本项目已为此写过实现，示例不再造第四份。
//
// 画法口径（示意式，不做地理投影）：线路只走水平/垂直/2:1 斜线（字符格约 1:2，
// 2:1 斜线看起来才是 45°）；一格只能有一个前景色，所以换乘站用中性色 `◉`，
// 让两条线在这里"交汇"而不是互相覆盖。
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/route/junction.hpp"
#include "termchart/layout/label_avoid.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/util/wcwidth.hpp"

namespace {

using namespace termchart;

constexpr char32_t kHorizontal = U'\u2500';  // ─
constexpr char32_t kVertical = U'\u2502';    // │
constexpr char32_t kSlashUp = U'\u2571';     // ╱（向右上升）
constexpr char32_t kSlashDown = U'\u2572';   // ╲
constexpr char32_t kStation = U'\u25CF';     // ● 普通站
constexpr char32_t kInterchange = U'\u25C9'; // ◉ 换乘站

// 地图整体下移一行，把第 0 行留给标题。
constexpr double kTopOffset = 1.0;

struct Station {
    Point at;
    std::string name;
};

struct Line {
    std::string name;
    Color color;
    std::vector<Station> stations;  // 相邻两项之间画一段
};

// 网络拓扑：一条横线、一条竖线、一条 2:1 斜线，外加一圈环线。
// 关键约束（第一版踩过）：**同名必须同址**——同名不同址会被当成两座站，
// 图上就会出现两个"西站"；而环线不能与 1 号线共用同一条 y，否则两条线重叠成一条。
std::vector<Line> make_network() {
    return {
        // 1 号线：横贯中线 y=7
        Line{"1号线", Color::ansi16(1),
             {{Point{6, 7}, "西站"}, {Point{14, 7}, "人民广场"}, {Point{22, 7}, "中山路"},
              {Point{30, 7}, "体育中心"}, {Point{38, 7}, "东站"}}},
        // 2 号线：竖穿中线 x=22，两端各与环线换乘
        Line{"2号线", Color::ansi16(2),
             {{Point{22, 3}, "文化宫"}, {Point{22, 7}, "中山路"}, {Point{22, 11}, "南门"},
              {Point{22, 15}, "南站"}}},
        // 3 号线：2:1 斜线（字符格约 1:2，2:1 看起来才是 45°）
        Line{"3号线", Color::ansi16(3),
             {{Point{6, 3}, "西苑"}, {Point{14, 7}, "人民广场"}, {Point{22, 11}, "南门"},
              {Point{30, 15}, "机场"}}},
        // 环线：矩形回路 6..38 × 3..15；四角与 1/2 号线的端点换乘
        Line{"环线", Color::ansi16(4),
             {{Point{6, 7}, "西站"}, {Point{6, 3}, "西苑"}, {Point{22, 3}, "文化宫"},
              {Point{38, 3}, "东苑"}, {Point{38, 7}, "东站"}, {Point{38, 15}, "东湖"},
              {Point{30, 15}, "机场"}, {Point{22, 15}, "南站"}, {Point{6, 15}, "西南新城"},
              {Point{6, 7}, "西站"}}},
    };
}

// 同址合并后，每座站被几条线路引用。车站数与换乘站数**共用这一次统计**，
// 免得两处各算一遍再互相矛盾（第一版标题硬编码 7、正文算出 6，就是这么来的）。
std::vector<int> station_usage(const std::vector<Station>& all) {
    std::vector<std::pair<int, int>> unique;
    std::vector<int> counts;
    for (const Station& s : all) {
        const int x = static_cast<int>(s.at.x);
        const int y = static_cast<int>(s.at.y);
        std::size_t index = unique.size();
        for (std::size_t i = 0; i < unique.size(); ++i) {
            if (unique[i].first == x && unique[i].second == y) {
                index = i;
                break;
            }
        }
        if (index == unique.size()) {
            unique.emplace_back(x, y);
            counts.push_back(0);
        }
        ++counts[index];
    }
    return counts;
}

std::size_t unique_station_count(const std::vector<Station>& all) {
    return station_usage(all).size();
}

// 换乘站 = 被两条及以上线路引用的同址站。
std::size_t count_interchanges(const std::vector<Station>& all) {
    const std::vector<int> usage = station_usage(all);
    return static_cast<std::size_t>(
        std::count_if(usage.begin(), usage.end(), [](int c) { return c > 1; }));
}

// 逐个走格子（本示例的线段只有三种：水平、竖直、2:1 斜线）。
// 之所以不用 canvas.draw_line：这里需要把**画过的每一格**记进占用集，供标签避让使用。
//
// 三种情况必须分开写：第一版统一按"x 步进、y 插值"处理，于是**竖直段的 x 也每步 +1**，
// 画出来不是竖线而是一条向右的阶梯（渲染出来一眼就看出来了）。
template <typename Fn>
void walk_cells(Point a, Point b, Fn&& fn) {
    const int x0 = static_cast<int>(a.x);
    const int y0 = static_cast<int>(a.y);
    const int x1 = static_cast<int>(b.x);
    const int y1 = static_cast<int>(b.y);

    if (y0 == y1) {  // 水平
        const int step = x1 >= x0 ? 1 : -1;
        for (int x = x0;; x += step) {
            fn(x, y0);
            if (x == x1) {
                break;
            }
        }
        return;
    }
    if (x0 == x1) {  // 竖直
        const int step = y1 >= y0 ? 1 : -1;
        for (int y = y0;; y += step) {
            fn(x0, y);
            if (y == y1) {
                break;
            }
        }
        return;
    }
    // 斜线：x 步进、y 按比例取整（本示例的斜率恰好是 2:1，取整无损）
    const int steps = std::abs(x1 - x0);
    const int dir = x1 >= x0 ? 1 : -1;
    for (int i = 0; i <= steps; ++i) {
        fn(x0 + dir * i, y0 + (y1 - y0) * i / steps);
    }
}

char32_t segment_char(Point a, Point b) {
    if (static_cast<int>(a.y) == static_cast<int>(b.y)) {
        return kHorizontal;
    }
    if (static_cast<int>(a.x) == static_cast<int>(b.x)) {
        return kVertical;
    }
    // 向右走且 y 增大 = 屏幕上的下坡 → ╲；否则 ╱。
    const bool down = (b.x > a.x) == (b.y > a.y);
    return down ? kSlashDown : kSlashUp;
}

}  // namespace

int main() {
    // 宽度按**标题与图例的显示宽度**取（CJK 每字 2 列），别按字符个数估——
    // 第一版取 48 列，标题直接被裁掉半行。
    canvas::Canvas canvas(66, 19);
    const Style title_style = Style{}.set_fg(Color::ansi16(7)).set_attr(Attr::Bold);
    const Style text_style = Style{}.set_fg(Color::ansi16(7));
    const Style dim_style = Style{}.set_fg(Color::ansi16(8));
    const Style interchange_style = Style{}.set_fg(Color::ansi16(15)).set_attr(Attr::Bold);

    const std::vector<Line> network = make_network();
    std::vector<Station> every_station;
    for (const Line& line : network) {
        for (const Station& s : line.stations) {
            every_station.push_back(s);
        }
    }
    // 标题里的数字**算出来**，不硬编码（第一版硬编码成 7，实际统计是 6，自相矛盾）。
    const std::size_t station_total = unique_station_count(every_station);
    const std::size_t interchange_total = count_interchanges(every_station);
    char title[160] = {};
    std::snprintf(title, sizeof(title), "城市轨道交通示意图  4 条线路 · %zu 座车站 · %zu 座换乘站",
                  station_total, interchange_total);
    canvas.draw_text(Point{1.0, 0.0}, title, title_style);

    // 站点表：同名同址的站合并成一座（换乘站自然就是被多条线引用的那座）。
    struct StationInfo {
        Point at;
        std::string name;
        int lines = 0;
    };
    std::vector<StationInfo> stations;
    const auto station_index = [&stations](const Station& s) {
        for (std::size_t i = 0; i < stations.size(); ++i) {
            if (static_cast<int>(stations[i].at.x) == static_cast<int>(s.at.x) &&
                static_cast<int>(stations[i].at.y) == static_cast<int>(s.at.y)) {
                return i;
            }
        }
        stations.push_back(StationInfo{s.at, s.name, 0});
        return stations.size() - 1;
    };

    std::vector<Rect> occupied;  // 供标签避让：线路格 + 车站格
    const auto occupy = [&occupied](int x, int y) {
        occupied.push_back(Rect{static_cast<double>(x), static_cast<double>(y) + kTopOffset, 1.0,
                                1.0});
    };

    // 1) 线路：先画线（车站随后覆盖上去），并把画过的格记进占用集。
    for (const Line& line : network) {
        const Style style = Style{}.set_fg(line.color);
        for (std::size_t i = 0; i + 1 < line.stations.size(); ++i) {
            const Point a{line.stations[i].at.x, line.stations[i].at.y + kTopOffset};
            const Point b{line.stations[i + 1].at.x, line.stations[i + 1].at.y + kTopOffset};
            const char32_t cp = segment_char(a, b);
            walk_cells(a, b, [&](int x, int y) {
                if (x < 0 || y < 0) {
                    return;
                }
                canvas.put(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), cp, style);
                occupy(x, y);
            });
        }
        for (const Station& s : line.stations) {
            (void)station_index(s);
        }
    }

    // 2) 车站：普通站 ● 用线路色，换乘站 ◉ 用中性色（一格只能有一个前景色）。
    for (const Line& line : network) {
        for (const Station& s : line.stations) {
            const std::size_t index = station_index(s);
            ++stations[index].lines;
        }
    }
    const auto row_of = [](const StationInfo& info) { return info.at.y + kTopOffset; };
    for (const StationInfo& info : stations) {
        const bool interchange = info.lines > 1;
        Style style = interchange ? interchange_style : Style{}.set_fg(Color::ansi16(7));
        if (!interchange) {
            for (const Line& line : network) {
                for (const Station& s : line.stations) {
                    if (static_cast<int>(s.at.x) == static_cast<int>(info.at.x) &&
                        static_cast<int>(s.at.y) == static_cast<int>(info.at.y)) {
                        style = Style{}.set_fg(line.color);
                    }
                }
            }
        }
        canvas.put(static_cast<std::uint32_t>(info.at.x), static_cast<std::uint32_t>(row_of(info)),
                   interchange ? kInterchange : kStation, style);
        occupy(static_cast<int>(info.at.x), static_cast<int>(row_of(info)));
    }

    // 3) 站名：**批量交给公共实现**（occuped 含线路与车站格，成功者自动进占用集，
    //    所以标签之间也不会互相压）。放不下就不画 —— 与图表侧的标签口径一致。
    std::vector<layout::LabelBox> labels;
    for (const StationInfo& info : stations) {
        layout::LabelBox box;
        box.anchor = Point{info.at.x, row_of(info)};
        box.width = static_cast<std::uint32_t>(util::display_width(info.name));
        labels.push_back(box);
    }
    layout::LabelAvoidOptions options;
    options.gap = 1.0;
    options.bounds = Rect{0.0, 0.0, static_cast<double>(canvas.width()),
                          static_cast<double>(canvas.height())};
    const std::vector<layout::LabelSlot> slots = layout::place_labels(labels, occupied, options);

    std::size_t placed = 0;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (!slots[i].placed) {
            continue;  // 放不下：明确不画，而不是压在线路上
        }
        ++placed;
        const Style style = stations[i].lines > 1 ? interchange_style : text_style;
        canvas.draw_text(Point{slots[i].rect.x, slots[i].rect.y}, stations[i].name, style);
    }

    // 4) 图例 + 统计（放在最底两行，避开地图本体）。
    const double legend_y = static_cast<double>(canvas.height()) - 2.0;
    const auto legend_row = static_cast<std::uint32_t>(legend_y);
    canvas.put(1u, legend_row, kStation, text_style);
    canvas.draw_text(Point{3.0, legend_y}, "车站", dim_style);
    canvas.put(8u, legend_row, kInterchange, interchange_style);
    canvas.draw_text(Point{10.0, legend_y}, "换乘站", dim_style);
    double legend_x = 19.0;
    for (const Line& line : network) {
        canvas.put(static_cast<std::uint32_t>(legend_x),
                   static_cast<std::uint32_t>(legend_y), kHorizontal,
                   Style{}.set_fg(line.color));
        canvas.draw_text(Point{legend_x + 2.0, legend_y}, line.name,
                         Style{}.set_fg(line.color));
        legend_x += 2.0 + static_cast<double>(util::display_width(line.name)) + 2.0;
    }

    char summary[160] = {};
    std::snprintf(summary, sizeof(summary), "%zu 站 · %zu 换乘 · 站名 %zu/%zu 落位（放不下不画）",
                  station_total, interchange_total, placed, stations.size());
    canvas.draw_text(Point{1.0, static_cast<double>(canvas.height()) - 1.0}, summary, dim_style);

    render::Renderer renderer;
    const Status status = api::write_stdout(renderer.render_full(canvas.to_buffer()));
    (void)api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
