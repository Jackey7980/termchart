// label.cpp —— 轴名标签实现
#include "termchart/layout/label.hpp"

#include "internal.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/layout/canvas_sink.hpp"
#include "termchart/layout/sink.hpp"

namespace termchart::layout {

void draw_axis_label(Sink& sink, std::string_view text, const Rect& rect, Side side,
                     HAlign halign, const Style& style) {
    if (text.empty()) {
        return;
    }
    switch (side) {
        case Side::Bottom: {
            const double start = align_start(rect.x, rect.right(), text.size(), halign);
            sink.text(Point{start, rect.bottom() + 2.0}, text, style);
            break;
        }
        case Side::Left: {
            const double start = rect.x - 1.0 - static_cast<double>(text.size());
            sink.text(Point{start, rect.bottom() - 1.0}, text, style);
            break;
        }
        case Side::Right: {
            // 右轴名：贴在 inner 右边之外，垂直居中于整条轴。
            const double mid_y = (rect.y + rect.bottom()) / 2.0;
            const double start = align_start(rect.right() + 2.0,
                                             rect.right() + 2.0 + static_cast<double>(text.size()),
                                             text.size(), halign);
            sink.text(Point{start, mid_y}, text, style);
            break;
        }
        case Side::Top:
            break;
    }
}

void draw_axis_label(canvas::Canvas& canvas, std::string_view text, const Rect& rect, Side side,
                     HAlign halign, const Style& style) {
    CanvasSink sink(canvas);
    draw_axis_label(sink, text, rect, side, halign, style);
}

namespace {

// 两版抽稀的公共体：用"上一保留项的右沿"与"当前项的左沿"判定最小间隔。
// 位置版把每个标签宽度视为 0（纯按刻度间距抽），宽度版用真实标签半宽。
void thin_tick_labels_impl(const std::vector<double>& positions,
                           const std::vector<std::size_t>* widths,
                           double min_gap_cells, std::vector<std::size_t>& out) {
    out.clear();
    if (positions.empty()) {
        return;
    }
    if (!(min_gap_cells > 0.0)) {
        // 间隔非正：保留全部。
        for (std::size_t i = 0; i < positions.size(); ++i) {
            out.push_back(i);
        }
        return;
    }
    out.push_back(0);  // 始终保留第一个
    double prev_right = positions[0];
    if (widths != nullptr && !widths->empty()) {
        prev_right += static_cast<double>((*widths)[0]) / 2.0;
    }
    for (std::size_t i = 1; i < positions.size(); ++i) {
        const double half = (widths != nullptr && i < widths->size())
                                ? static_cast<double>((*widths)[i]) / 2.0
                                : 0.0;
        const double left = positions[i] - half;
        if (left - prev_right >= min_gap_cells) {
            out.push_back(i);
            prev_right = positions[i] + half;
        }
    }
}

}  // namespace

std::vector<std::size_t> thin_tick_labels(const std::vector<double>& positions,
                                          double min_gap_cells) {
    std::vector<std::size_t> out;
    thin_tick_labels_impl(positions, nullptr, min_gap_cells, out);
    return out;
}

std::vector<std::size_t> thin_tick_labels(const std::vector<double>& positions,
                                          const std::vector<std::size_t>& widths,
                                          double min_gap_cells) {
    std::vector<std::size_t> out;
    thin_tick_labels_impl(positions, &widths, min_gap_cells, out);
    return out;
}

}  // namespace termchart::layout
