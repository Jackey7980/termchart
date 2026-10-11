// annotation.cpp —— 参考线 / 区域高亮 / 标注实现
#include "termchart/layout/annotation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/layout/label_avoid.hpp"
#include "termchart/scale/linear.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::layout {
namespace {

// 取该方向对应的比例尺。
const scale::LinearScale& axis_scale(const PlotArea& area,
                                     Orientation orientation) noexcept {
    return orientation == Orientation::Horizontal ? area.y_scale() : area.x_scale();
}

// 把数据坐标映射成格坐标。两条规则，都必须遵守：
//   * **域外返回 false**：比例尺默认带 clamp，域外的值会被夹到边界行，看起来像
//     一条真实存在的参考线 —— 那是在骗读者，宁可什么都不画；
//   * **域内必须夹进 [low, high-1]**：比例尺把域端点映射到 inner.bottom()/right()，
//     那是格坐标的半开边界，落在有效行/列之外。不夹的话，y 轴最低值（或 x 轴最大值）
//     的参考线与高亮整条会消失。
bool cell_of_value(const PlotArea& area, Orientation orientation, double value,
                   std::uint32_t& out) noexcept {
    const scale::LinearScale& scale = axis_scale(area, orientation);
    if (value < scale.domain_min() || value > scale.domain_max()) {
        return false;
    }
    const Rect inner = area.inner_rect();
    const double low = orientation == Orientation::Horizontal ? inner.y : inner.x;
    const double high = orientation == Orientation::Horizontal ? inner.bottom()
                                                              : inner.right();
    const double last = high - 1.0;
    if (last < low) {
        return false;  // 绘图区在该方向不足一格
    }
    const double cell = std::floor(scale.forward(value));
    const double clamped = std::min(std::max(cell, low), last);
    out = static_cast<std::uint32_t>(clamped);
    return true;
}

}  // namespace

bool draw_reference_line(canvas::Canvas& canvas, const PlotArea& area,
                         const ReferenceLine& line) {
    std::uint32_t fixed = 0;
    if (!cell_of_value(area, line.orientation, line.value, fixed)) {
        return false;
    }
    const Rect inner = area.inner_rect();
    if (line.orientation == Orientation::Horizontal) {
        for (double x = inner.x; x < inner.right(); x += 1.0) {
            canvas.put(Point{x, static_cast<double>(fixed)}, line.cp, line.style);
        }
        return true;
    }
    for (double y = inner.y; y < inner.bottom(); y += 1.0) {
        canvas.put(Point{static_cast<double>(fixed), y}, line.cp, line.style);
    }
    return true;
}

std::size_t draw_highlight_band(canvas::Canvas& canvas, const PlotArea& area,
                                const HighlightBand& band) {
    // 先把数据区间按**数据域**判断与夹取，再映射成格：域外的部分直接截掉，
    // 而不是让比例尺的 clamp 把整条带吸到边界上。
    const scale::LinearScale& scale = axis_scale(area, band.orientation);
    const double low_value = std::min(band.from, band.to);
    const double high_value = std::max(band.from, band.to);
    if (high_value < scale.domain_min() || low_value > scale.domain_max()) {
        return 0;  // 整条带在域外
    }
    const double domain_low = std::max(low_value, scale.domain_min());
    const double domain_high = std::min(high_value, scale.domain_max());

    std::uint32_t first = 0;
    std::uint32_t second = 0;
    if (!cell_of_value(area, band.orientation, domain_low, first) ||
        !cell_of_value(area, band.orientation, domain_high, second)) {
        return 0;
    }
    if (first > second) {
        std::swap(first, second);
    }
    const Rect inner = area.inner_rect();

    std::size_t touched = 0;
    if (band.orientation == Orientation::Horizontal) {
        for (std::uint32_t row = first; row <= second; ++row) {
            for (double x = inner.x; x < inner.right(); x += 1.0) {
                const auto col = static_cast<std::uint32_t>(x);
                const Cell& cell = canvas.at(col, row);
                if (is_continuation(cell)) {
                    continue;  // 宽字符续接格：由头部统一着色
                }
                if (band.fill_char != U'\0') {
                    canvas.put(col, row, band.fill_char, cell.style);
                } else {
                    Style style = cell.style;
                    style.set_bg(band.color);
                    canvas.put(col, row, cell.cp, style);
                }
                ++touched;
            }
        }
        return touched;
    }

    for (std::uint32_t col = first; col <= second; ++col) {
        for (double y = inner.y; y < inner.bottom(); y += 1.0) {
            const auto row = static_cast<std::uint32_t>(y);
            const Cell& cell = canvas.at(col, row);
            if (is_continuation(cell)) {
                continue;
            }
            if (band.fill_char != U'\0') {
                canvas.put(col, row, band.fill_char, cell.style);
            } else {
                Style style = cell.style;
                style.set_bg(band.color);
                canvas.put(col, row, cell.cp, style);
            }
            ++touched;
        }
    }
    return touched;
}

bool draw_annotation(canvas::Canvas& canvas, const PlotArea& area,
                     const Annotation& annotation, const std::vector<Rect>& occupied) {
    if (annotation.text.empty()) {
        return false;
    }
    const Point anchor = area.cell_of(annotation.at.x, annotation.at.y);
    LabelBox box;
    box.anchor = Point{std::floor(anchor.x), std::floor(anchor.y)};
    box.width = static_cast<std::uint32_t>(
        std::max(1, util::display_width(annotation.text)));
    box.height = 1;

    LabelAvoidOptions options;
    options.bounds = area.inner_rect();  // 标注只允许落在绘图区内
    const LabelSlot slot = place_label_avoiding(box, occupied, options);
    if (!slot.placed) {
        return false;
    }
    canvas.draw_text(Point{slot.rect.x, slot.rect.y}, annotation.text, annotation.style);
    return true;
}

}  // namespace termchart::layout
