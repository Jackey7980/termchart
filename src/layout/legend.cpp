// legend.cpp —— 图例实现
#include "termchart/layout/legend.hpp"

#include <cstddef>

#include "internal.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/layout/canvas_sink.hpp"
#include "termchart/layout/sink.hpp"

namespace termchart::layout {
namespace {

// 单项占宽：色块(1) + 间隔(1) + 名称 + 尾随间隙(2)。一行 k 项的总宽 = Σ - 2（去掉末项尾随）。
constexpr double entry_width(const LegendEntry& e) noexcept {
    return 1.0 + 1.0 + static_cast<double>(e.name.size()) + 2.0;
}

// 在 (x, y) 处画一整行图例（内部不重新对齐，start 已由调用方算好）。
void draw_legend_row(Sink& sink, std::span<const LegendEntry> entries,
                     const std::vector<std::size_t>& idx, double x, double y,
                     const Style& base) {
    for (const std::size_t i : idx) {
        const LegendEntry& e = entries[i];
        sink.put(Point{x, y}, U'\u2588', Style{}.set_fg(e.color));  // █
        x += 1.0;
        sink.put(Point{x, y}, U'\u0020', base);
        x += 1.0;
        sink.text(Point{x, y}, e.name, base);
        x += static_cast<double>(e.name.size());
        x += 2.0;
    }
}

}  // namespace

void draw_legend(Sink& sink, std::span<const LegendEntry> entries, const Rect& rect, HAlign halign,
                 const Style& base) {
    if (entries.empty()) {
        return;
    }
    std::size_t total = 0;
    for (const auto& e : entries) {
        total += 1 + 1 + e.name.size() + 2;
    }
    if (total > 0) {
        total -= 2;
    }
    const double start = align_start(rect.x, rect.right(), total, halign);
    double x = start;
    const double y = rect.y;
    for (const auto& e : entries) {
        sink.put(Point{x, y}, U'\u2588', Style{}.set_fg(e.color));  // █
        x += 1.0;
        sink.put(Point{x, y}, U'\u0020', base);
        x += 1.0;
        sink.text(Point{x, y}, e.name, base);
        x += static_cast<double>(e.name.size());
        x += 2.0;
    }
}

std::vector<std::vector<std::size_t>> legend_rows(std::span<const LegendEntry> entries,
                                                  double max_width_cells) {
    std::vector<std::vector<std::size_t>> rows;
    if (entries.empty()) {
        return rows;
    }
    const double limit = (max_width_cells > 0.0) ? max_width_cells : 1e9;
    std::vector<std::size_t> cur;
    double cur_w = 0.0;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const double w = entry_width(entries[i]);
        if (cur.empty()) {
            cur.push_back(i);
            cur_w = w - 2.0;  // 首行/首行单项去掉尾随间隙
        } else if (cur_w + w <= limit) {
            cur.push_back(i);
            cur_w += w;
        } else {
            rows.push_back(std::move(cur));
            cur.push_back(i);
            cur_w = w - 2.0;
        }
    }
    if (!cur.empty()) {
        rows.push_back(std::move(cur));
    }
    return rows;
}

void draw_legend(Sink& sink, std::span<const LegendEntry> entries, const Rect& rect,
                 const LegendOptions& opts, const Style& base) {
    if (entries.empty()) {
        return;
    }
    const double limit = (opts.max_width_cells > 0.0) ? opts.max_width_cells : rect.w;
    const std::vector<std::vector<std::size_t>> rows = legend_rows(entries, limit);
    double y = rect.y;
    for (const std::vector<std::size_t>& row : rows) {
        double row_w = 0.0;
        for (const std::size_t i : row) {
            row_w += entry_width(entries[i]);
        }
        row_w = std::max(row_w - 2.0, 0.0);
        const double start = align_start(rect.x, rect.right(), row_w, opts.halign);
        draw_legend_row(sink, entries, row, start, y, base);
        y += 1.0;  // 下一行向下堆叠
    }
}

void draw_legend(canvas::Canvas& canvas, std::span<const LegendEntry> entries, const Rect& rect,
                 HAlign halign, const Style& base) {
    CanvasSink sink(canvas);
    draw_legend(sink, entries, rect, halign, base);
}

void draw_legend(canvas::Canvas& canvas, std::span<const LegendEntry> entries, const Rect& rect,
                 const LegendOptions& opts, const Style& base) {
    CanvasSink sink(canvas);
    draw_legend(sink, entries, rect, opts, base);
}

}  // namespace termchart::layout
