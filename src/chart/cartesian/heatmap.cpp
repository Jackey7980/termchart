// heatmap.cpp —— 笛卡尔热力图实现（先算网格，再逐格上色）
#include "termchart/chart/cartesian/heatmap.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/detail/block_chars.hpp"
#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

constexpr int kMaxGridDim = 128;  // 每个方向的格数上限，防病态输入爆内存

// 归一化计数 t → 密度字符（1×1 块）：点数越多越"实"。最低档也**非空格**，否则
// "有点但很少"的格会与"无数据"无法区分。
char32_t heat_char(double t) noexcept {
    constexpr char32_t kLevels[4] = {detail::kLightShade, detail::kMediumShade,
                                     detail::kDarkShade, detail::kFullBlock};
    int idx = static_cast<int>(t * 4.0);
    idx = std::clamp(idx, 0, 3);
    return kLevels[static_cast<std::size_t>(idx)];
}

// 水平色标：在内框上方行画一条从冷到热的色带 + 名称，帮助读者把颜色读回数量。
void draw_colorbar(canvas::Canvas& canvas, const Rect& inner, color::ColormapKind kind,
                   const std::string& name) {
    const int len = static_cast<int>(std::min(24.0, inner.w));
    if (len < 1) {
        return;  // 内框过窄：宁可不画，也不折回左侧
    }
    const double x0 = std::floor(inner.x);
    for (int i = 0; i < len; ++i) {
        const double t = len <= 1 ? 0.0
                                  : static_cast<double>(i) / static_cast<double>(len - 1);
        const Rgb rgb = color::sample(kind, t);
        canvas.put(Point{x0 + static_cast<double>(i), 0.0}, detail::kFullBlock,
                   Style{}.set_fg(Color::rgb(rgb.r, rgb.g, rgb.b)));
    }
    if (!name.empty()) {
        canvas.draw_text(Point{x0 + static_cast<double>(len) + 1.0, 0.0}, name, Style{});
    }
}

}  // namespace

void HeatmapChart::set_values(std::vector<Point> points, color::ColormapKind kind,
                              std::string name) {
    points_ = std::move(points);
    kind_ = kind;
    name_ = std::move(name);
}

void HeatmapChart::draw(DrawContext ctx) {
    // 只保留有限点：NaN/Inf 会污染 min/max（进而污染定义域）与落格索引。
    std::vector<Point> pts;
    pts.reserve(points_.size());
    for (const Point& p : points_) {
        if (std::isfinite(p.x) && std::isfinite(p.y)) {
            pts.push_back(p);
        }
    }
    if (pts.empty()) {
        return;  // 无有效点：不绘制，也避免零宽定义域
    }

    double xmin = pts.front().x;
    double xmax = pts.front().x;
    double ymin = pts.front().y;
    double ymax = pts.front().y;
    for (const Point& p : pts) {
        xmin = std::min(xmin, p.x);
        xmax = std::max(xmax, p.x);
        ymin = std::min(ymin, p.y);
        ymax = std::max(ymax, p.y);
    }
    // 全部同值（或单点）：给单位宽/高，避免零宽定义域与除零。
    if (!(xmax > xmin)) {
        xmax = xmin + 1.0;
    }
    if (!(ymax > ymin)) {
        ymax = ymin + 1.0;
    }
    const double span_x = xmax - xmin;
    const double span_y = ymax - ymin;

    // 决定网格步长：显式 cell_size 优先；否则按点数自动——每个方向约
    // ceil(sqrt(n)) 格（每格平均落约 1 个点），clamp 到 [1, kMaxGridDim]。
    double step_x = cell_size_;
    double step_y = cell_size_;
    if (!(step_x > 0.0) || !(step_y > 0.0)) {
        const double k = std::ceil(std::sqrt(static_cast<double>(pts.size())));
        const double steps = std::clamp(k, 1.0, static_cast<double>(kMaxGridDim));
        step_x = span_x / steps;
        step_y = span_y / steps;
    }

    const int cols = std::clamp(static_cast<int>(std::ceil(span_x / step_x)), 1, kMaxGridDim);
    const int rows = std::clamp(static_cast<int>(std::ceil(span_y / step_y)), 1, kMaxGridDim);

    // 实际单元尺寸：让网格**恰好**覆盖定义域，落格索引不会因 step 与 span/格数
    // 之间的误差而越界。
    const double cell_w = span_x / static_cast<double>(cols);
    const double cell_h = span_y / static_cast<double>(rows);

    // 先算：计数矩阵。索引一律 clamp，右端点会落到 cols/rows，钳进末格——绝不
    // 产生负索引或越界下标。
    const auto dim_cols = static_cast<std::size_t>(cols);
    const auto dim_rows = static_cast<std::size_t>(rows);
    std::vector<std::size_t> count(dim_cols * dim_rows, 0);
    for (const Point& p : pts) {
        int col = static_cast<int>(std::floor((p.x - xmin) / cell_w));
        int row = static_cast<int>(std::floor((p.y - ymin) / cell_h));
        col = std::clamp(col, 0, cols - 1);
        row = std::clamp(row, 0, rows - 1);
        ++count[static_cast<std::size_t>(row) * dim_cols + static_cast<std::size_t>(col)];
    }
    std::size_t max_count = 0;
    for (std::size_t c : count) {
        max_count = std::max(max_count, c);
    }
    if (max_count == 0) {
        return;  // 理论上不可达（pts 非空），保守兜底
    }

    // 定义域精确取数据范围：网格边界与画布严格对齐，避免 nice_domain 的留白让
    // 格子错位。
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, xmin, xmax, ymin, ymax, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());

    // 网格先画作背景：空格（无数据）上仍能看见网格参照。
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }

    // 逐格上色。矩形先与内框求交（越界裁剪），空交跳过——即使内框为负尺寸、
    // 或格数远多于画布列数，也不会把负坐标折回第 0 列。
    const Rect inner = area_.inner_rect();
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const std::size_t idx =
                static_cast<std::size_t>(row) * dim_cols + static_cast<std::size_t>(col);
            const std::size_t c = count[idx];
            if (c == 0) {
                continue;  // 留空（保留网格线）
            }
            const double dx0 = xmin + cell_w * static_cast<double>(col);
            const double dy0 = ymin + cell_h * static_cast<double>(row);
            // 数据 y 向上增长、画布行向下增长：单元顶端 dy1 映射到更小的行。
            const double px0 = area_.x_scale().forward(dx0);
            const double px1 = area_.x_scale().forward(dx0 + cell_w);
            const double py0 = area_.y_scale().forward(dy0 + cell_h);
            const double py1 = area_.y_scale().forward(dy0);
            const Rect cell_rect =
                Rect{px0, py0, px1 - px0, py1 - py0}.intersection(inner);
            if (cell_rect.empty()) {
                continue;  // 完全落在内框之外
            }
            const double t = static_cast<double>(c) / static_cast<double>(max_count);
            const Rgb rgb = color::sample(kind_, t);
            ctx.canvas.fill_rect(cell_rect, heat_char(t),
                                 Style{}.set_fg(Color::rgb(rgb.r, rgb.g, rgb.b)));
        }
    }

    // 轴画在数据**之后**：热力图几乎铺满内框，若先画轴，最左列数据会盖掉 y 轴。
    if (ctx.draw_axes) {
        const Style axis_style = ctx.axis_style;
        layout::draw_axis_x(ctx.canvas, area_, xticks, "x", axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "y", axis_style);
    }

    if (show_legend_ && ctx.draw_legend) {
        draw_colorbar(ctx.canvas, inner, kind_, name_);
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

}  // namespace termchart::chart
