// compositor.cpp —— 多图层合成实现
#include "termchart/render/compositor.hpp"

#include <algorithm>

#include "termchart/core/cell.hpp"

namespace termchart::render {

CellBuffer Compositor::composite(const std::vector<Layer>& layers) {
    std::int32_t max_x = 0;
    std::int32_t max_y = 0;
    for (const Layer& l : layers) {
        if (l.buffer == nullptr) {
            continue;
        }
        max_x = std::max(max_x, l.x + static_cast<std::int32_t>(l.buffer->width()));
        max_y = std::max(max_y, l.y + static_cast<std::int32_t>(l.buffer->height()));
    }
    CellBuffer out(static_cast<std::uint32_t>(max_x), static_cast<std::uint32_t>(max_y));
    composite(layers, out);
    return out;
}

void Compositor::composite(const std::vector<Layer>& layers, CellBuffer& out) {
    for (const Layer& l : layers) {
        if (l.buffer == nullptr) {
            continue;
        }
        const CellBuffer& src = *l.buffer;
        for (std::uint32_t y = 0; y < src.height(); ++y) {
            for (std::uint32_t x = 0; x < src.width(); ++x) {
                const Cell& c = src.at(x, y);
                const std::int32_t ox = l.x + static_cast<std::int32_t>(x);
                const std::int32_t oy = l.y + static_cast<std::int32_t>(y);
                if (ox < 0 || oy < 0) {
                    continue;
                }
                if (l.transparent && c.cp == U' ') {
                    continue;  // 空格透出下层
                }
                out.at(static_cast<std::uint32_t>(ox), static_cast<std::uint32_t>(oy)) = c;
            }
        }
    }
}

}  // namespace termchart::render
