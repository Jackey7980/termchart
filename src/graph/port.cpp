// port.cpp —— 端口锚点几何
#include "termchart/graph/port.hpp"

#include <algorithm>
#include <cmath>

#include "termchart/util/math.hpp"

namespace termchart::graph {

Point port_anchor(Rect box, PortSide side, double t) noexcept {
    const double u = util::clamp(t, 0.0, 1.0);
    // 侧向坐标按**格索引**取整：宽 w 的边有 w 个格，t=0.5 落在中间格。
    const double span_x = std::max(0.0, box.w - 1.0);
    const double span_y = std::max(0.0, box.h - 1.0);
    const double lateral_x = box.x + std::floor(span_x * u);
    const double lateral_y = box.y + std::floor(span_y * u);

    switch (side) {
        case PortSide::North:
            return Point{lateral_x, box.y};
        case PortSide::South:
            return Point{lateral_x, box.bottom() - 1.0};
        case PortSide::West:
            return Point{box.x, lateral_y};
        case PortSide::East:
            return Point{box.right() - 1.0, lateral_y};
    }
    return Point{box.x, box.y};
}

PortSide opposite(PortSide side) noexcept {
    switch (side) {
        case PortSide::North:
            return PortSide::South;
        case PortSide::South:
            return PortSide::North;
        case PortSide::East:
            return PortSide::West;
        case PortSide::West:
            return PortSide::East;
    }
    return PortSide::North;
}

PortSide side_towards(Point from, Point to) noexcept {
    const double dx = to.x - from.x;
    const double dy = to.y - from.y;
    if (std::fabs(dx) >= std::fabs(dy)) {
        return dx >= 0.0 ? PortSide::East : PortSide::West;
    }
    return dy >= 0.0 ? PortSide::South : PortSide::North;
}

}  // namespace termchart::graph
