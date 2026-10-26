// table.cpp —— 数据表格实现：显示宽度列宽 + 整列裁剪
#include "termchart/chart/text/table.hpp"

#include "termchart/chart/scene_sink.hpp"

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

#include "termchart/core/attr.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::chart {
namespace {

constexpr char32_t kSepV = U'\u2502';  // │
constexpr char32_t kSepH = U'\u2500';  // ─
constexpr char32_t kSepT = U'\u253C';  // ├（右端同字符近似 ┤，格粒度）

Rect content_area(const DrawContext& ctx, const Chart& chart) {
    const double w = static_cast<double>(ctx.canvas.width());
    const double h = static_cast<double>(ctx.canvas.height());
    Rect area{chart.margin_left(), chart.margin_top(),
              std::max(0.0, w - chart.margin_left() - chart.margin_right()),
              std::max(0.0, h - chart.margin_top() - chart.margin_bottom())};
    if (ctx.draw_title && !chart.title().empty() && area.h >= 2.0) {
        layout::draw_title(ctx.canvas, chart.title(), area);
        area = Rect{area.x, area.y + 1.0, area.w, area.h - 1.0};
    }
    return area;
}

// 按显示宽度截断到 budget 列（prefix_by_width 保证不切在组合记号中间）。
std::string_view clip(std::string_view text, int budget) noexcept {
    if (budget <= 0) {
        return {};
    }
    return text.substr(0, util::prefix_by_width(text, budget));
}

}  // namespace

void Table::add_column(std::string name) { columns_.push_back(std::move(name)); }

void Table::add_row(std::vector<std::string> cells) {
    rows_.push_back(std::move(cells));
}

void Table::draw(DrawContext ctx) {
    const std::size_t n = columns_.size();
    if (n == 0) {
        return;
    }

    // 列宽：表头与该列所有单元格的最大显示宽度。
    std::vector<int> width(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        width[i] = util::display_width(columns_[i]);
    }
    for (const auto& row : rows_) {
        for (std::size_t i = 0; i < n && i < row.size(); ++i) {
            width[i] = std::max(width[i], util::display_width(row[i]));
        }
    }

    const Rect area = content_area(ctx, *this);
    if (area.w < 1.0 || area.h < 1.0) {
        return;
    }

    // 整列裁剪：从右往左丢弃放不下的列（列间留 1 列分隔位），至少保一列。
    std::size_t kept = 0;
    double used = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double need = used + static_cast<double>(width[i]) + (i > 0 ? 1.0 : 0.0);
        if (i > 0 && need > area.w) {
            break;
        }
        used = need;
        kept = i + 1;
    }

    const Style header = Style{}.set_attr(Attr::Bold);
    const Style body = Style{};

    double row_y = area.y;
    double col_x = area.x;
    double table_right = col_x;
    for (std::size_t i = 0; i < kept; ++i) {
        if (i > 0) {
            ctx.canvas.put(Point{col_x, row_y}, kSepV, body);
            col_x += 1.0;
        }
        ctx.canvas.draw_text(Point{col_x, row_y}, clip(columns_[i], width[i]), header);
        col_x += static_cast<double>(width[i]);
        table_right = col_x;
    }

    // 表头分隔线：与表格同宽，两端近似用 ├ ┤。
    row_y += 1.0;
    if (row_y < area.bottom() && table_right > area.x + 1.0) {
        ctx.canvas.put(Point{area.x, row_y}, kSepT, body);
        for (double x = area.x + 1.0; x < table_right - 1.0; x += 1.0) {
            ctx.canvas.put(Point{x, row_y}, kSepH, body);
        }
        ctx.canvas.put(Point{table_right - 1.0, row_y}, kSepT, body);
        row_y += 1.0;
    }

    for (const auto& row : rows_) {
        if (row_y >= area.bottom()) {
            return;
        }
        col_x = area.x;
        for (std::size_t i = 0; i < kept; ++i) {
            if (i > 0) {
                ctx.canvas.put(Point{col_x, row_y}, kSepV, body);
                col_x += 1.0;
            }
            const std::string_view cell =
                i < row.size() ? std::string_view{row[i]} : std::string_view{};
            ctx.canvas.draw_text(Point{col_x, row_y}, clip(cell, width[i]), body);
            col_x += static_cast<double>(width[i]);
        }
        row_y += 1.0;
    }
}

bool Table::to_scene(scene::Scene& out, std::uint32_t width,
                     std::uint32_t height) const {
    const std::size_t n = columns_.size();
    if (n == 0) {
        return true;
    }

    // 列宽：表头与该列所有单元格的最大显示宽度。
    std::vector<int> colw(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        colw[i] = util::display_width(columns_[i]);
    }
    for (const auto& row : rows_) {
        for (std::size_t i = 0; i < n && i < row.size(); ++i) {
            colw[i] = std::max(colw[i], util::display_width(row[i]));
        }
    }

    SceneSink sink(out);
    const double W = static_cast<double>(width);
    const double H = static_cast<double>(height);
    Rect area{margin_left(), margin_top(),
              std::max(0.0, W - margin_left() - margin_right()),
              std::max(0.0, H - margin_top() - margin_bottom())};
    if (!title_.empty() && area.h >= 2.0) {
        layout::draw_title(sink, title_, area);
        area = Rect{area.x, area.y + 1.0, area.w, area.h - 1.0};
    }
    if (area.w < 1.0 || area.h < 1.0) {
        return true;
    }

    // 整列裁剪：从右往左丢弃放不下的列（至少保一列）。
    std::size_t kept = 0;
    double used = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double need = used + static_cast<double>(colw[i]) + (i > 0 ? 1.0 : 0.0);
        if (i > 0 && need > area.w) {
            break;
        }
        used = need;
        kept = i + 1;
    }

    const Style header = Style{}.set_attr(Attr::Bold);
    const Style body = Style{};

    scene::SceneBuilder& sb = sink.builder();
    sb.begin_group(0);

    double row_y = area.y;
    double col_x = area.x;
    double table_right = col_x;
    for (std::size_t i = 0; i < kept; ++i) {
        if (i > 0) {
            sink.put(Point{col_x, row_y}, kSepV, body);
            col_x += 1.0;
        }
        sink.text(Point{col_x, row_y}, clip(columns_[i], colw[i]), header);
        col_x += static_cast<double>(colw[i]);
        table_right = col_x;
    }

    // 表头分隔线：与表格同宽，两端近似用 ├ ┤。
    row_y += 1.0;
    if (row_y < area.bottom() && table_right > area.x + 1.0) {
        sink.put(Point{area.x, row_y}, kSepT, body);
        for (double x = area.x + 1.0; x < table_right - 1.0; x += 1.0) {
            sink.put(Point{x, row_y}, kSepH, body);
        }
        sink.put(Point{table_right - 1.0, row_y}, kSepT, body);
        row_y += 1.0;
    }

    for (const auto& row : rows_) {
        if (row_y >= area.bottom()) {
            break;
        }
        col_x = area.x;
        for (std::size_t i = 0; i < kept; ++i) {
            if (i > 0) {
                sink.put(Point{col_x, row_y}, kSepV, body);
                col_x += 1.0;
            }
            const std::string_view cell =
                i < row.size() ? std::string_view{row[i]} : std::string_view{};
            sink.text(Point{col_x, row_y}, clip(cell, colw[i]), body);
            col_x += static_cast<double>(colw[i]);
        }
        row_y += 1.0;
    }

    sb.end_group(0);
    return true;
}

}  // namespace termchart::chart
