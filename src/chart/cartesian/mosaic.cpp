// mosaic.cpp —— 马赛克图实现（列宽按边际、列内按条件占比堆叠）
#include "termchart/chart/cartesian/mosaic.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "termchart/canvas/canvas.hpp"
#include "termchart/detail/block_chars.hpp"
#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/legend.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"
#include "termchart/chart/scene_sink.hpp"

namespace termchart::chart {
namespace {

Color row_color(std::size_t r) noexcept {
    static constexpr std::array<int, 6> kPalette{4, 2, 3, 5, 1, 6};
    return Color::ansi16(static_cast<std::uint8_t>(kPalette[r % kPalette.size()]));
}

}  // namespace

void MosaicChart::set_counts(std::vector<std::vector<double>> counts,
                             std::vector<std::string> row_labels,
                             std::vector<std::string> col_labels) {
    counts_ = std::move(counts);
    row_labels_ = std::move(row_labels);
    col_labels_ = std::move(col_labels);
}

void MosaicChart::draw(DrawContext ctx) {
    const auto rows = counts_.size();
    if (rows == 0 || counts_.front().empty()) {
        return;
    }
    const auto cols = counts_.front().size();
    // 行长不一致的矩阵按最短行截断，超出部分丢弃（马赛克要求规整矩阵）。
    for (const std::vector<double>& r : counts_) {
        if (r.size() != cols) {
            return;  // 锯齿矩阵不画，避免静默给出误导性比例
        }
    }

    // 计数钳到 [0,∞)：负计数无意义。
    double total = 0.0;
    std::vector<double> col_sums(cols, 0.0);
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            const double v = std::max(0.0, counts_[r][c]);
            col_sums[c] += v;
            total += v;
        }
    }
    if (!(total > 0.0)) {
        return;  // 全零矩阵无比例可言
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, 0.0, 1.0, 0.0, 1.0, false, false);
    const Rect inner = area_.inner_rect();

    double xc = inner.x;
    for (std::size_t c = 0; c < cols; ++c) {
        const double w = inner.w * (col_sums[c] / total);
        double yb = inner.bottom();  // 自下而上堆叠
        for (std::size_t r = 0; r < rows; ++r) {
            if (col_sums[c] <= 0.0) {
                break;
            }
            const double h = inner.h * (std::max(0.0, counts_[r][c]) / col_sums[c]);
            if (h > 0.0) {
                const Rect seg =
                    Rect{xc, yb - h, w, h}.intersection(inner);
                if (!seg.empty()) {
                    ctx.canvas.fill_rect(seg, detail::kFullBlock,
                                         Style{}.set_fg(row_color(r)));
                }
            }
            yb -= h;
        }
        // 列边界分隔线（跳过最右缘）。
        if (options_.show_separators && c + 1 < cols) {
            ctx.canvas.draw_line(Point{xc + w, inner.y}, Point{xc + w, inner.bottom() - 1.0},
                                 U'\u2502', Style{}.set_fg(default_color(DefaultColor::Muted)));
        }
        xc += w;
    }

    if (options_.show_legend && ctx.draw_legend) {
        std::vector<layout::LegendEntry> entries;
        entries.reserve(rows);
        for (std::size_t r = 0; r < rows; ++r) {
            const std::string& name =
                r < row_labels_.size() ? row_labels_[r] : "row" + std::to_string(r);
            entries.push_back(layout::LegendEntry{name, row_color(r)});
        }
        layout::draw_legend(ctx.canvas, entries, Rect{inner.x, 0.0, inner.w, 1.0},
                            layout::HAlign::Left);
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

bool MosaicChart::to_scene(scene::Scene& out, std::uint32_t width,
                           std::uint32_t height) const {
    const auto rows = counts_.size();
    if (rows == 0 || counts_.front().empty()) {
        return true;
    }
    const auto cols = counts_.front().size();
    // 行长不一致的矩阵按最短行截断：超出部分丢弃（马赛克要求规整矩阵）。
    for (const std::vector<double>& r : counts_) {
        if (r.size() != cols) {
            return true;  // 锯齿矩阵不画，避免静默给出误导性比例
        }
    }

    // 计数钳到 [0,∞)：负计数无意义。
    double total = 0.0;
    std::vector<double> col_sums(cols, 0.0);
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            const double v = std::max(0.0, counts_[r][c]);
            col_sums[c] += v;
            total += v;
        }
    }
    if (!(total > 0.0)) {
        return true;  // 全零矩阵无比例可言
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(width, height, m, 0.0, 1.0, 0.0, 1.0, false, false);
    const Rect inner = area_.inner_rect();

    // 马赛克本无固定数值轴；按范式补一层 [0,1] 网格 + 轴框，便于对齐。
    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    SceneSink sink(out);
    layout::draw_grid(sink, area_, xticks, yticks,
                      Style{}.set_fg(default_color(DefaultColor::Muted)));
    layout::draw_axis_x(sink, area_, xticks, "x", Style{});
    layout::draw_axis_y(sink, area_, yticks, "y", Style{});

    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);
    double xc = inner.x;
    for (std::size_t c = 0; c < cols; ++c) {
        const double w = inner.w * (col_sums[c] / total);
        double yb = inner.bottom();  // 自下而上堆叠
        for (std::size_t r = 0; r < rows; ++r) {
            if (col_sums[c] <= 0.0) {
                break;
            }
            const double h = inner.h * (std::max(0.0, counts_[r][c]) / col_sums[c]);
            if (h > 0.0) {
                const Rect seg = Rect{xc, yb - h, w, h}.intersection(inner);
                if (!seg.empty()) {
                    b.fill(row_color(r));
                    b.rect(seg);
                }
            }
            yb -= h;
        }
        // 列边界分隔线（跳过最右缘）。
        if (options_.show_separators && c + 1 < cols) {
            b.stroke(default_color(DefaultColor::Muted));
            b.move_to(Point{xc + w, inner.y});
            b.line_to(Point{xc + w, inner.bottom() - 1.0});
        }
        xc += w;
    }

    if (options_.show_legend) {
        std::vector<layout::LegendEntry> entries;
        entries.reserve(rows);
        for (std::size_t r = 0; r < rows; ++r) {
            const std::string& name =
                r < row_labels_.size() ? row_labels_[r] : "row" + std::to_string(r);
            entries.push_back(layout::LegendEntry{name, row_color(r)});
        }
        layout::draw_legend(sink, entries, Rect{inner.x, 0.0, inner.w, 1.0},
                            layout::HAlign::Left);
    }
    if (!title_.empty()) {
        layout::draw_title(sink, title_, inner);
    }
    b.end_group(0);
    return true;
}

}  // namespace termchart::chart
