// 03_function_plot.cpp —— 高级示例：多曲线 + 图例 + 坐标刻度（阶段 2 能力上限演示）
//
// 在一个 PlotArea 上同时绘制 sin(x) 与 cos(x)：
//   * 坐标轴 / 网格 / 刻度 / 标题由 layout/ 原语完成；
//   * 每条曲线写进自己的 FloatField，用盲文光栅器（2×4 子像素）渲染；
//   * 图例由 layout::draw_legend 在顶部绘制。
#include <cmath>
#include <cstdio>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/image/field.hpp"
#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/label.hpp"
#include "termchart/layout/layout.hpp"
#include "termchart/layout/legend.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/raster/rasterizer.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/scale/ticks.hpp"
#include "termchart/term/terminal.hpp"

namespace {

constexpr std::uint32_t kCanvasW = 80;
constexpr std::uint32_t kCanvasH = 26;
constexpr double kXMax = 6.28318530717958647692 * 2.0;  // 两个周期
constexpr char32_t kBrailleBlank = U'\u2800';            // 盲文熄灭格码点

// 把光栅化出的曲线层叠加到画布 inner 区域（仅覆盖点亮的子像素）。
void blit(const termchart::CellBuffer& wave, termchart::canvas::Canvas& canvas,
          const termchart::Rect& inner) {
    const std::uint32_t ox = static_cast<std::uint32_t>(inner.x);
    const std::uint32_t oy = static_cast<std::uint32_t>(inner.y);
    for (std::uint32_t cy = 0; cy < wave.height(); ++cy)
        for (std::uint32_t cx = 0; cx < wave.width(); ++cx) {
            const termchart::Cell& c = wave.at(cx, cy);
            if (c.cp != kBrailleBlank)
                canvas.put(ox + cx, oy + cy, c.cp, c.style);
        }
}

template <typename F>
termchart::CellBuffer render_curve(const termchart::layout::PlotArea& area, F fn,
                                   termchart::Color color) {
    const termchart::Rect inner = area.inner_rect();
    const std::uint32_t inner_w = static_cast<std::uint32_t>(inner.w);
    const std::uint32_t inner_h = static_cast<std::uint32_t>(inner.h);
    termchart::image::FloatField field(inner_w * 2u, inner_h * 4u);
    const std::uint32_t samples = inner_w * 8u;  // 远超子像素密度，保证连续
    for (std::uint32_t i = 0; i <= samples; ++i) {
        const double x = kXMax * static_cast<double>(i) / static_cast<double>(samples);
        const double y = fn(x);
        const termchart::Point g = area.cell_of(x, y);
        const std::uint32_t sx = static_cast<std::uint32_t>(std::lround((g.x - inner.x) * 2.0));
        const std::uint32_t sy = static_cast<std::uint32_t>(std::lround((g.y - inner.y) * 4.0));
        if (sx < field.width() && sy < field.height())
            field.set(sx, sy, 1.0f);
    }
    termchart::raster::RasterOptions opts;
    opts.fg = color;
    return termchart::raster::rasterize(field, termchart::raster::RasterKind::Braille, opts);
}

}  // namespace

int main() {
    termchart::term::Terminal terminal(termchart::term::TerminalOptions{
        .alt_screen = false, .hide_cursor = false});
    if (terminal.enter() != termchart::ErrorCode::Ok) {
        std::fprintf(stderr, "无法进入终端模式\n");
        return 1;
    }

    termchart::layout::PlotArea area(kCanvasW, kCanvasH, termchart::layout::Margin{}, 0.0, kXMax,
                                     -1.2, 1.2);
    termchart::canvas::Canvas canvas(kCanvasW, kCanvasH);
    const termchart::Rect inner = area.inner_rect();

    std::vector<double> xticks = termchart::scale::nice_ticks(0.0, kXMax);
    std::vector<double> yticks = termchart::scale::nice_ticks(-1.2, 1.2);

    termchart::Style axis;
    axis.set_fg(termchart::Color::ansi16(8));  // 灰
    termchart::layout::draw_grid(canvas, area, xticks, yticks, axis);
    termchart::layout::draw_axis_x(canvas, area, xticks, "x", axis);
    termchart::layout::draw_axis_y(canvas, area, yticks, "y", axis);

    termchart::Style title;
    title.set_fg(termchart::Color::rgb(143, 165, 190)).set_attr(termchart::Attr::Bold);
    termchart::layout::draw_title(
        canvas, "sin(x) & cos(x)",
        termchart::Rect{0.0, 0.0, static_cast<double>(kCanvasW), 1.0},
        termchart::layout::HAlign::Left, title);

    std::vector<termchart::layout::LegendEntry> legend = {
        {"sin", termchart::Color::rgb(86, 197, 154)},
        {"cos", termchart::Color::rgb(246, 193, 90)},
    };
    termchart::layout::draw_legend(
        canvas,
        std::span<const termchart::layout::LegendEntry>{legend.data(), legend.size()},
        termchart::Rect{46.0, 0.0, 30.0, 1.0},
        termchart::layout::HAlign::Right);

    blit(render_curve(area, [](double x) { return std::sin(x); },
                      termchart::Color::rgb(86, 197, 154)),
         canvas, inner);
    blit(render_curve(area, [](double x) { return std::cos(x); },
                      termchart::Color::rgb(246, 193, 90)),
         canvas, inner);

    termchart::render::Renderer renderer(terminal.caps().color_depth);
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    if (termchart::api::write_stdout(bytes) != termchart::ErrorCode::Ok) return 1;
    if (termchart::api::write_stdout("\n") != termchart::ErrorCode::Ok) return 1;
    (void)termchart::api::flush_stdout();
    return terminal.leave() == termchart::ErrorCode::Ok ? 0 : 1;
}
