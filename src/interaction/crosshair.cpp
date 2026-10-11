// crosshair.cpp —— 十字准线实现（纯绘制 + 还原，无终端/渲染依赖）
#include "termchart/interaction/crosshair.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

#include "termchart/interaction/detail/overlay_cells.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::interaction {
namespace {

// 浮点 → 非负格坐标：负值/NaN 归 0，极大值饱和到 uint32 上限（避免 UB）。
// 与 canvas 的落格口径一致（越界最终由画布丢弃）。
std::uint32_t floor_cell(double value) noexcept {
    if (!(value > 0.0)) {
        return 0;
    }
    const double floored = std::floor(value);
    constexpr double kMax = 4294967295.0;  // uint32 上限
    if (floored >= kMax) {
        return std::numeric_limits<std::uint32_t>::max();
    }
    return static_cast<std::uint32_t>(floored);
}

// 把数据格的"保留字符、只改样式"合成出来：line_style 里已设的前景/背景才覆盖
// 原值，属性位按或并入。这样默认（只设了背景）不会动用字符与前景。
Style overlay_preserving(const Style& base, const Style& line) noexcept {
    Style result = base;
    if (!line.fg().is_none()) {
        result.set_fg(line.fg());
    }
    if (!line.bg().is_none()) {
        result.set_bg(line.bg());
    }
    const Attr attrs[] = {Attr::Bold, Attr::Dim, Attr::Italic, Attr::Underline,
                          Attr::Reverse};
    for (const Attr attr : attrs) {
        if (line.has_attr(attr)) {
            result.set_attr(attr, true);
        }
    }
    return result;
}

}  // namespace

Crosshair::Crosshair(CrosshairOptions options) noexcept : options_(options) {}

void Crosshair::set_options(CrosshairOptions options) noexcept { options_ = options; }

const CrosshairOptions& Crosshair::options() const noexcept { return options_; }

// 簿记三件套统一走 detail::overlay_cells.hpp（crosshair 与 tooltip 共用唯一实现）。
void Crosshair::reset_captured(std::uint32_t width, std::uint32_t height) {
    captured_w_ = width;
    captured_h_ = height;
    detail::begin_log(patches_, captured_, width, height);
}

void Crosshair::capture(const canvas::Canvas& canvas, std::uint32_t x, std::uint32_t y) {
    detail::capture_cell(canvas, patches_, captured_, captured_w_, x, y);
}

bool Crosshair::restore(canvas::Canvas& canvas) {
    const bool restored = detail::restore_cells(canvas, patches_, captured_);
    if (restored) {
        captured_w_ = 0;
        captured_h_ = 0;
    }
    return restored;
}

bool Crosshair::draw(canvas::Canvas& canvas, Point at, std::string_view readout) {
    restore(canvas);  // 幂等：先还原上一次，再按新点重绘。

    const Rect& plot = options_.plot_rect;
    // 退化：绘图区任一方向不足 2 格（含 1×1、单行、单列），十字退化成点/线，不画。
    if (!(plot.w >= 2.0) || !(plot.h >= 2.0)) {
        return false;
    }
    // 绘图区原点为负说明不在画布坐标系内，同样不画（避免"区外格被改写"的歧义）。
    if (plot.x < 0.0 || plot.y < 0.0) {
        return false;
    }
    // at 必须落在绘图区内（半开区间）；区外一律不画。
    if (!plot.contains(at)) {
        return false;
    }

    reset_captured(canvas.width(), canvas.height());

    const std::uint32_t x0 = floor_cell(plot.x);
    const std::uint32_t x1 = floor_cell(plot.right());   // 半开：末列 = x1 - 1
    const std::uint32_t y0 = floor_cell(plot.y);
    const std::uint32_t y1 = floor_cell(plot.bottom());
    const std::uint32_t col = floor_cell(at.x);
    const std::uint32_t row = floor_cell(at.y);

    // 横线：整行 [x0, x1)；竖线：整列 [y0, y1)。交叉格由竖线覆盖横线的样式（一致，
    // 因为两者同一 line_style；字符在 preserve_data 下根本不改）。
    for (std::uint32_t x = x0; x < x1; ++x) {
        capture(canvas, x, row);
    }
    for (std::uint32_t y = y0; y < y1; ++y) {
        capture(canvas, col, y);
    }

    for (std::uint32_t x = x0; x < x1; ++x) {
        if (x >= canvas.width() || row >= canvas.height()) {
            continue;
        }
        const Cell before = canvas.at(x, row);
        const char32_t cp = options_.preserve_data ? before.cp : options_.horizontal_cp;
        const Style style =
            options_.preserve_data ? overlay_preserving(before.style, options_.line_style)
                                   : options_.line_style;
        canvas.put(x, row, cp, style);
    }
    for (std::uint32_t y = y0; y < y1; ++y) {
        if (col >= canvas.width() || y >= canvas.height()) {
            continue;
        }
        const Cell before = canvas.at(col, y);
        const bool is_cross = (y == row);
        const char32_t cp =
            options_.preserve_data
                ? before.cp
                : (is_cross ? options_.cross_cp : options_.vertical_cp);
        const Style style =
            options_.preserve_data ? overlay_preserving(before.style, options_.line_style)
                                   : options_.line_style;
        canvas.put(col, y, cp, style);
    }

    // 轴侧读数：写在绘图区**下边框的下方**（唯一允许出绘图区的元素），列对齐准线。
    if (options_.show_readout) {
        const std::string text =
            readout.empty() ? ("x=" + std::to_string(col) + " y=" + std::to_string(row))
                            : std::string(readout);
        const std::uint32_t readout_row = y1;
        const int width = util::display_width(text);
        if (width > 0 && readout_row < canvas.height()) {
            // 先记录将被文本覆盖的列段（含宽字符的续接列），再落笔。
            for (int i = 0; i < width; ++i) {
                const std::uint32_t x = col + static_cast<std::uint32_t>(i);
                capture(canvas, x, readout_row);
            }
            canvas.draw_text(Point{static_cast<double>(col),
                                   static_cast<double>(readout_row)},
                             text, options_.line_style);
        }
    }

    return !patches_.empty();
}

bool Crosshair::clear(canvas::Canvas& canvas) { return restore(canvas); }

bool Crosshair::active() const noexcept { return !patches_.empty(); }

std::size_t Crosshair::patch_count() const noexcept { return patches_.size(); }

}  // namespace termchart::interaction
