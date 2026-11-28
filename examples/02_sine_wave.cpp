// 02_sine_wave.cpp —— 浮点画布 → 盲文子像素 → CellBuffer → 终端（阶段 2 出口示例）
//
// 与初版的区别：波形不再用 canvas.put('*') 在字符网格上逐点打（1 字符 = 1 采样），
// 而是把曲线写进 image::FloatField（连续强度场），再用 raster/ 的盲文光栅器
// （每字符 2×4 = 8 子像素）压成 CellBuffer，精度比原版高约 8 倍。
// 坐标轴 / 标题由 layout/ 原语在 Canvas 上绘制，盲文曲线叠加其上。
#include <cmath>
#include <cstdio>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/media/image/field.hpp"
#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/label.hpp"
#include "termchart/layout/layout.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/raster/rasterizer.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/scale/ticks.hpp"
#include "termchart/term/terminal.hpp"

namespace {

constexpr std::uint32_t kCanvasW = 72;
constexpr std::uint32_t kCanvasH = 24;
constexpr double kXMax = 6.28318530717958647692 * 2.0;  // 两个周期
constexpr char32_t kBrailleBlank = U'\u2800';            // 盲文熄灭格码点

// 把光栅化出的波形层叠加到画布 inner 区域（仅覆盖点亮的子像素）。
void blit(const termchart::CellBuffer& wave, termchart::canvas::Canvas& canvas,
          const termchart::Rect& inner) {
    const std::uint32_t ox = static_cast<std::uint32_t>(inner.x);
    const std::uint32_t oy = static_cast<std::uint32_t>(inner.y);
    for (std::uint32_t cy = 0; cy < wave.height(); ++cy) {
        for (std::uint32_t cx = 0; cx < wave.width(); ++cx) {
            const termchart::Cell& c = wave.at(cx, cy);
            if (c.cp != kBrailleBlank) {
                canvas.put(ox + cx, oy + cy, c.cp, c.style);
            }
        }
    }
}

termchart::CellBuffer render_wave(const termchart::layout::PlotArea& area,
                                  termchart::Color color) {
    const termchart::Rect inner = area.inner_rect();
    const std::uint32_t inner_w = static_cast<std::uint32_t>(inner.w);
    const std::uint32_t inner_h = static_cast<std::uint32_t>(inner.h);
    termchart::image::FloatField field(inner_w * 2u, inner_h * 4u);
    const std::uint32_t samples = inner_w * 8u;  // 远超子像素密度，保证连续
    for (std::uint32_t i = 0; i <= samples; ++i) {
        const double x = kXMax * static_cast<double>(i) / static_cast<double>(samples);
        const double y = std::sin(x);
        const termchart::Point g = area.cell_of(x, y);
        const std::uint32_t sx = static_cast<std::uint32_t>(std::lround((g.x - inner.x) * 2.0));
        const std::uint32_t sy = static_cast<std::uint32_t>(std::lround((g.y - inner.y) * 4.0));
        if (sx < field.width() && sy < field.height()) {
            field.set(sx, sy, 1.0f);
        }
    }
    termchart::raster::RasterOptions opts;
    opts.fg = color;
    return termchart::raster::rasterize(field, termchart::raster::RasterKind::Braille, opts);
}

}  // namespace

int main() {
    // RAII：异常退出（含 return 之外的路径）也会恢复终端。
    termchart::term::Terminal terminal(termchart::term::TerminalOptions{
        .alt_screen = false, .hide_cursor = false});
    if (terminal.enter() != termchart::ErrorCode::Ok) {
        std::fprintf(stderr, "无法进入终端模式\n");
        return 1;
    }

    termchart::layout::PlotArea area(kCanvasW, kCanvasH, termchart::layout::Margin{}, 0.0, kXMax,
                                     -1.0, 1.0);
    termchart::canvas::Canvas canvas(kCanvasW, kCanvasH);
    const termchart::Rect inner = area.inner_rect();

    std::vector<double> xticks = termchart::scale::nice_ticks(0.0, kXMax);
    std::vector<double> yticks = termchart::scale::nice_ticks(-1.0, 1.0);

    termchart::Style axis;
    axis.set_fg(termchart::Color::ansi16(8));  // 灰
    termchart::layout::draw_grid(canvas, area, xticks, yticks, axis);
    termchart::layout::draw_axis_x(canvas, area, xticks, "x", axis);
    termchart::layout::draw_axis_y(canvas, area, yticks, "y", axis);

    termchart::Style title;
    title.set_fg(termchart::Color::rgb(86, 197, 154)).set_attr(termchart::Attr::Bold);
    termchart::layout::draw_title(canvas, "sin(x)", inner, termchart::layout::HAlign::Center, title);
    termchart::layout::draw_axis_label(canvas, "x", inner, termchart::layout::Side::Bottom);
    termchart::layout::draw_axis_label(canvas, "y", inner, termchart::layout::Side::Left);

    blit(render_wave(area, termchart::Color::rgb(86, 197, 154)), canvas, inner);

    termchart::render::Renderer renderer(terminal.caps().color_depth);
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    if (termchart::api::write_stdout(bytes) != termchart::ErrorCode::Ok) return 1;
    if (termchart::api::write_stdout("\n") != termchart::ErrorCode::Ok) return 1;
    (void)termchart::api::flush_stdout();
    return terminal.leave() == termchart::ErrorCode::Ok ? 0 : 1;
}
