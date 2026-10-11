// arrow.cpp —— 箭头头部实现
#include "termchart/graph/arrow.hpp"

namespace termchart::graph {

char32_t arrow_head_char(PortSide dir) noexcept {
    switch (dir) {
        case PortSide::East:
            return U'\u25B6';  // BLACK RIGHT-POINTING TRIANGLE
        case PortSide::West:
            return U'\u25C0';  // BLACK LEFT-POINTING TRIANGLE
        case PortSide::North:
            return U'\u25B2';  // BLACK UP-POINTING TRIANGLE
        case PortSide::South:
            return U'\u25BC';  // BLACK DOWN-POINTING TRIANGLE
    }
    return U'\u25B6';
}

std::vector<Point> arrow_head_cells(Point tip, PortSide dir) {
    Point behind = tip;
    switch (dir) {
        case PortSide::East:
            behind = Point{tip.x - 1.0, tip.y};
            break;
        case PortSide::West:
            behind = Point{tip.x + 1.0, tip.y};
            break;
        case PortSide::North:
            behind = Point{tip.x, tip.y + 1.0};
            break;
        case PortSide::South:
            behind = Point{tip.x, tip.y - 1.0};
            break;
    }
    return {behind, tip};
}

}  // namespace termchart::graph
