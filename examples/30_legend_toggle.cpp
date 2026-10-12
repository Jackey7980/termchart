// 30_legend_toggle.cpp —— 图例点击切换（规格示例，阶段 5 出口条件之一）
//
// 与 21/22 不同，本示例**只走脚本模式**：直接构造"点击坐标"驱动 LegendToggle，
// 不经过 InputParser 的字节链——那部分 21 已演示过，这里聚焦图例语义本身：
//   * 图例项是 HitRegion::rect 且 payload = series id（几何归画图方，语义归交互方）；
//   * 点击后**下一帧**少画被隐藏的系列——"隐藏"不是渲染特效，而是数据层过滤；
//   * 图例自身显示切换状态（[x] 隐藏 / [ ] 可见），与 LegendToggle 状态同源。
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/hit_region.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/interaction/legend_toggle.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

constexpr double kX0 = 0.0;
constexpr double kX1 = 30.0;
constexpr int kSamples = 90;

struct Series {
    std::uint32_t id;
    std::string label;
    Style style;
    std::vector<Point> points;
};

// 绘图区：左侧 6 列留 y 轴刻度，底部 2 行留 x 轴。
Rect plot_area(const canvas::Canvas& canvas) {
    const double w = static_cast<double>(canvas.width());
    const double h = static_cast<double>(canvas.height());
    return Rect{6.0, 1.0, w - 7.0, h - 3.0};
}

Point to_canvas(const Rect& area, Point data, double y0, double y1) {
    const double nx = (data.x - kX0) / (kX1 - kX0);
    const double ny = (data.y - y0) / (y1 - y0);
    return Point{area.x + nx * area.w, area.y + (1.0 - ny) * area.h};
}

std::vector<Point> make_series(double (*fn)(double)) {
    std::vector<Point> points;
    points.reserve(static_cast<std::size_t>(kSamples) + 1);
    for (int i = 0; i <= kSamples; ++i) {
        const double x = kX0 + (kX1 - kX0) * static_cast<double>(i) / static_cast<double>(kSamples);
        points.push_back(Point{x, fn(x)});
    }
    return points;
}

// 图例盒：两行条目，右上角。返回每项的 HitRegion（payload = series id）。
std::vector<chart::HitRegion> draw_legend(canvas::Canvas& canvas,
                                          const std::vector<Series>& series,
                                          const interaction::LegendToggle& legend) {
    const double w = static_cast<double>(canvas.width());
    const std::uint32_t x0 = static_cast<std::uint32_t>(w) - 12;
    canvas.fill_rect(Rect{static_cast<double>(x0) - 1.0, 0.0, 13.0, 4.0}, U' ');
    canvas.draw_text(Point{static_cast<double>(x0), 0.0}, "图例", Style{}.set_attr(Attr::Bold));

    std::vector<chart::HitRegion> regions;
    std::uint32_t row = 1;
    for (const Series& item : series) {
        const bool visible = legend.visible(item.id);
        const std::string marker = visible ? "[ ] " : "[x] ";
        canvas.draw_text(Point{static_cast<double>(x0), static_cast<double>(row)},
                         marker + item.label, item.style);
        regions.push_back(chart::HitRegion::rect(
            Rect{static_cast<double>(x0), static_cast<double>(row), 12.0, 1.0}, item.id));
        ++row;
    }
    return regions;
}

}  // namespace

int main() {
    // 两条曲线（同一坐标域，y ∈ [-1.2, 1.2]）。
    std::vector<Series> series{
        {1, "sin", Style{}.set_fg(Color::ansi16(6)), make_series([](double x) {
             return 0.9 * std::sin(0.6 * x);
         })},
        {2, "cos", Style{}.set_fg(Color::ansi16(3)), make_series([](double x) {
             return 0.9 * std::cos(0.5 * x);
         })},
    };

    interaction::LegendToggle legend;
    for (const Series& item : series) {
        legend.register_series(item.id, item.label);
    }

    const auto draw_frame = [&](const char* caption) -> std::vector<chart::HitRegion> {
        canvas::Canvas canvas(56, 14);
        const Rect area = plot_area(canvas);

        // 轴：横轴 + 纵轴。
        canvas.draw_line(Point{area.x, area.bottom()},
                         Point{area.right(), area.bottom()}, U'-');
        canvas.draw_line(Point{area.x, area.y}, Point{area.x, area.bottom()}, U'|');
        canvas.draw_text(Point{area.x + 2.0, area.bottom() + 1.0},
                         "0                          30", Style{});

        // 只画可见系列："隐藏"是数据层过滤，不是透明度特效。
        for (const Series& item : series) {
            if (!legend.visible(item.id)) {
                continue;
            }
            for (const Point& data : item.points) {
                const Point cell = to_canvas(area, data, -1.2, 1.2);
                canvas.put(static_cast<std::uint32_t>(cell.x), static_cast<std::uint32_t>(cell.y),
                           U'*', item.style);
            }
        }

        const std::vector<chart::HitRegion> legend_regions = draw_legend(canvas, series, legend);

        render::Renderer renderer;
        (void)api::write_stdout("\n== " + std::string(caption) + " ==\n");
        (void)api::write_stdout(renderer.render_full(canvas.to_buffer()));
        return legend_regions;
    };

    // ── 第 1 帧：全部可见 ────────────────────────────────────────────────
    std::vector<chart::HitRegion> legend_regions = draw_frame("初始：两条系列都可见");

    // ── 点击图例第 1 行（sin）→ 隐藏 ────────────────────────────────────
    // 脚本模式：坐标即图例第 1 行的任意一点（draw_legend 里 x0 = 56-12 = 44，row 1）。
    // 先验证"空命中不改变状态"（这是 click_at 的返回值语义：是否真的翻转了）。
    if (legend.click_at(legend_regions, 0.0, 0.0)) {
        std::fprintf(stderr, "空命中不应改变状态\n");
        return 1;
    }
    if (!legend.click_at(legend_regions, 46.0, 1.0)) {
        std::fprintf(stderr, "图例点击未生效\n");
        return 1;
    }
    legend_regions = draw_frame("点击 sin 的图例 → sin 隐藏");

    // ── 再点一次 → 恢复；然后隐藏 cos ───────────────────────────────────
    (void)legend.click_at(legend_regions, 46.0, 1.0);
    (void)legend.click_at(legend_regions, 46.0, 2.0);
    legend_regions = draw_frame("恢复 sin，点击 cos 的图例 → cos 隐藏");

    (void)api::flush_stdout();
    return 0;
}
