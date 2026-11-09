// 06_dashboard.cpp —— 画图能力展示台（line | bar | heatmap）
//
// 同一屏里同时画三种图，直接展示项目的"画图能力"：
//   ┌─────────────┬─────────────┐
//   │ 折线图       │ 柱状图       │   ← 上半屏：左线右柱
//   ├─────────────┴─────────────┤
//   │ 热力图（真彩半块）          │   ← 下半屏：全宽
//   └───────────────────────────┘
//
// 全部走能真正出图的 raster 旁路（image::FloatField → raster → canvas.put），
// 绕开 chart/graph 高级 API 那条阶段 7 才接 raster 的占位路径（见 ROADMAP R12），
// 所以坐标轴、网格、图例、彩色柱、真彩热力图都是"真"的，不是占位符。
//
// 跑法（Windows 终端，cmd）： chcp 65001 && build\ninja\examples\06_dashboard.exe
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/color/colormap.hpp"
#include "termchart/image/field.hpp"
#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/label.hpp"
#include "termchart/layout/layout.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/raster/rasterizer.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/scale/ticks.hpp"
#include "termchart/term/size.hpp"
#include "termchart/term/terminal.hpp"
#include <cstdlib>
#include <fstream>
#include "termchart/gui/cells.hpp"
#include "termchart/image/png_encode.hpp"

namespace {

constexpr double kXMax = 6.28318530717958647692 * 2.0;  // 两个周期
constexpr char32_t kBrailleBlank = U'\u2800';            // 盲文熄灭格
constexpr char32_t kFullBlock = U'\u2588';               // █
constexpr char32_t kUpperHalf = U'\u2580';               // ▀
constexpr char32_t kVLight = U'\u2502';                  // │
constexpr char32_t kHLight = U'\u2500';                  // ─

// hue∈[0,1] 经 Turbo 彩虹色表取色，shade 调制亮度。
termchart::Color colorize(double hue, double shade) {
    const termchart::Rgb base = termchart::color::sample(
        termchart::color::ColormapKind::Turbo, std::clamp(hue, 0.0, 1.0));
    const double b = 0.35 + 0.65 * shade;
    return termchart::Color::rgb(
        static_cast<std::uint8_t>(std::lround(base.r * b)),
        static_cast<std::uint8_t>(std::lround(base.g * b)),
        static_cast<std::uint8_t>(std::lround(base.b * b)));
}

void draw_text(termchart::canvas::Canvas& cv, const char* s, std::uint32_t row,
               std::uint32_t col0, const termchart::Style& st) {
    for (std::uint32_t i = 0; s[i] != '\0'; ++i) {
        cv.put(col0 + i, row, static_cast<char32_t>(s[i]), st);
    }
}

// 把临时面板整体拷到主画布（面板区域互不重叠，空格也照拷）。
void blit_panel(const termchart::canvas::Canvas& src,
                termchart::canvas::Canvas& dst, std::uint32_t ox,
                std::uint32_t oy) {
    const termchart::CellBuffer buf = src.to_buffer();
    for (std::uint32_t y = 0; y < buf.height(); ++y) {
        for (std::uint32_t x = 0; x < buf.width(); ++x) {
            const termchart::Cell& c = buf.at(x, y);
            dst.put(ox + x, oy + y, c.cp, c.style);
        }
    }
}

// ── 面板 A：折线图（盲文曲线 + 坐标轴 + 网格 + 图例）──
termchart::CellBuffer render_curve(const termchart::layout::PlotArea& area,
                                   double (*fn)(double), termchart::Color color) {
    const termchart::Rect inner = area.inner_rect();
    const std::uint32_t iw = static_cast<std::uint32_t>(inner.w);
    const std::uint32_t ih = static_cast<std::uint32_t>(inner.h);
    termchart::image::FloatField field(iw * 2u, ih * 4u);
    const std::uint32_t samples = iw * 8u;
    for (std::uint32_t i = 0; i <= samples; ++i) {
        const double x = kXMax * static_cast<double>(i) / static_cast<double>(samples);
        const double y = fn(x);
        const termchart::Point g = area.cell_of(x, y);
        const std::uint32_t sx =
            static_cast<std::uint32_t>(std::lround((g.x - inner.x) * 2.0));
        const std::uint32_t sy =
            static_cast<std::uint32_t>(std::lround((g.y - inner.y) * 4.0));
        if (sx < field.width() && sy < field.height()) {
            field.set(sx, sy, 1.0f);
        }
    }
    termchart::raster::RasterOptions opts;
    opts.fg = color;
    return termchart::raster::rasterize(field, termchart::raster::RasterKind::Braille,
                                        opts);
}

void blit_curve(termchart::canvas::Canvas& cv, const termchart::CellBuffer& wave,
                const termchart::Rect& inner) {
    const std::uint32_t ox = static_cast<std::uint32_t>(inner.x);
    const std::uint32_t oy = static_cast<std::uint32_t>(inner.y);
    for (std::uint32_t cy = 0; cy < wave.height(); ++cy) {
        for (std::uint32_t cx = 0; cx < wave.width(); ++cx) {
            const termchart::Cell& c = wave.at(cx, cy);
            if (c.cp == kBrailleBlank) continue;
            cv.put(ox + cx, oy + cy, c.cp, c.style);
        }
    }
}

void draw_line_chart(termchart::canvas::Canvas& cv, std::uint32_t w,
                     std::uint32_t h) {
    termchart::layout::PlotArea area(w, h, termchart::layout::Margin{}, 0.0, kXMax,
                                     -1.0, 1.0);
    const termchart::Rect inner = area.inner_rect();
    std::vector<double> xt = termchart::scale::nice_ticks(0.0, kXMax);
    std::vector<double> yt = termchart::scale::nice_ticks(-1.0, 1.0);
    termchart::Style axis;
    axis.set_fg(termchart::Color::ansi16(8));
    termchart::layout::draw_grid(cv, area, xt, yt, axis);
    termchart::layout::draw_axis_x(cv, area, xt, "x", axis);
    termchart::layout::draw_axis_y(cv, area, yt, "y", axis);
    termchart::Style title;
    title.set_fg(termchart::Color::rgb(120, 220, 255)).set_attr(termchart::Attr::Bold);
    termchart::layout::draw_title(cv, "line: sin & cos", inner,
                                  termchart::layout::HAlign::Center, title);
    termchart::layout::draw_axis_label(cv, "x", inner, termchart::layout::Side::Bottom);
    termchart::layout::draw_axis_label(cv, "y", inner, termchart::layout::Side::Left);

    blit_curve(cv, render_curve(area, std::sin, termchart::Color::rgb(86, 197, 154)),
               inner);
    blit_curve(cv, render_curve(area, std::cos, termchart::Color::rgb(255, 150, 90)),
               inner);

    termchart::Style ls;
    ls.set_fg(termchart::Color::rgb(86, 197, 154)).set_attr(termchart::Attr::Bold);
    draw_text(cv, "- sin", 1, static_cast<std::uint32_t>(inner.x) + 1, ls);
    ls = termchart::Style{};
    ls.set_fg(termchart::Color::rgb(255, 150, 90)).set_attr(termchart::Attr::Bold);
    draw_text(cv, "- cos", 1, static_cast<std::uint32_t>(inner.x) + 8, ls);
}

// ── 面板 B：柱状图（彩色柱 + y 轴 + 类别标签）──
void draw_bar_chart(termchart::canvas::Canvas& cv, std::uint32_t w,
                    std::uint32_t h) {
    constexpr std::uint32_t N = 8;
    const double data[N] = {0.85, 0.55, 0.92, 0.40, 0.70, 0.30, 0.62, 0.48};
    const std::uint32_t axis_x = 2;
    const std::uint32_t data_top = 2;
    const std::uint32_t baseline = (h > 3) ? h - 2u : h - 1u;
    double max_v = 0.0;
    for (std::uint32_t i = 0; i < N; ++i) {
        if (data[i] > max_v) max_v = data[i];
    }

    termchart::Style ax;
    ax.set_fg(termchart::Color::ansi16(8));
    for (std::uint32_t y = data_top; y <= baseline && y < h; ++y) {
        cv.put(axis_x, y, kVLight, ax);
    }
    for (std::uint32_t x = axis_x; x < w; ++x) {
        cv.put(x, baseline, kHLight, ax);
    }
    termchart::Style t;
    t.set_fg(termchart::Color::rgb(255, 200, 120)).set_attr(termchart::Attr::Bold);
    termchart::layout::draw_title(cv, "bar: samples",
                                  termchart::Rect{0.0, 0.0, static_cast<double>(w),
                                                  static_cast<double>(h)},
                                  termchart::layout::HAlign::Center, t);

    const std::uint32_t bar_area = (w > axis_x + 1) ? w - (axis_x + 1) : 0;
    const std::uint32_t bar_w = bar_area / N;
    const std::uint32_t usable_h = (baseline > data_top) ? baseline - data_top : 0;
    for (std::uint32_t i = 0; i < N; ++i) {
        const double fh = (data[i] / max_v) * static_cast<double>(usable_h);
        const std::uint32_t full = static_cast<std::uint32_t>(std::floor(fh));
        const double frac = fh - static_cast<double>(full);
        const std::uint32_t bx0 = axis_x + 1u + i * bar_w;
        const termchart::Color col = colorize(static_cast<double>(i) / static_cast<double>(N - 1), 1.0);
        termchart::Style s;
        s.set_fg(col);
        for (std::uint32_t r = 0; r < full && r < usable_h; ++r) {
            for (std::uint32_t x = bx0; x < bx0 + bar_w && x < w; ++x) {
                cv.put(x, baseline - 1u - r, kFullBlock, s);
            }
        }
        if (frac > 0.05 && full < usable_h) {
            for (std::uint32_t x = bx0; x < bx0 + bar_w && x < w; ++x) {
                cv.put(x, baseline - 1u - full, kUpperHalf, s);
            }
        }
        termchart::Style ls;
        ls.set_fg(termchart::Color::ansi16(7));
        draw_text(cv, std::to_string(i).c_str(), baseline + 1u, bx0, ls);
    }
}

// ── 面板 C：热力图（真彩半块，每字符上下两色）──
double rippled(double u, double v) {
    const double dx = u - 0.5, dy = v - 0.5;
    const double d = std::sqrt(dx * dx + dy * dy);
    return 0.5 + 0.5 * std::sin(12.0 * d) * std::cos(3.0 * std::atan2(dy, dx));
}

void draw_heatmap(termchart::canvas::Canvas& cv, std::uint32_t w,
                  std::uint32_t h) {
    termchart::Style t;
    t.set_fg(termchart::Color::rgb(180, 255, 180)).set_attr(termchart::Attr::Bold);
    termchart::layout::draw_title(cv, "heatmap: ripple field",
                                  termchart::Rect{0.0, 0.0, static_cast<double>(w),
                                                  static_cast<double>(h)},
                                  termchart::layout::HAlign::Center, t);
    if (h <= 1) return;
    const double denom = static_cast<double>(2 * (h - 1));
    for (std::uint32_t y = 1; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const double u = (w > 1) ? static_cast<double>(x) / static_cast<double>(w - 1) : 0.5;
            const double vt = static_cast<double>(y * 2) / denom;
            const double vb = static_cast<double>(y * 2 + 1) / denom;
            const termchart::Color ct = colorize(rippled(u, vt), 1.0);
            const termchart::Color cb = colorize(rippled(u, vb), 1.0);
            termchart::Style s;
            s.set_fg(ct).set_bg(cb);
            cv.put(x, y, kUpperHalf, s);  // ▀：上=fg(上层) 下=bg(下层)
        }
    }
}

}  // namespace

int main() {
    termchart::term::Terminal terminal(termchart::term::TerminalOptions{
        .alt_screen = false, .hide_cursor = false});
    if (terminal.enter() != termchart::ErrorCode::Ok) {
        std::fprintf(stderr, "无法进入终端模式\n");
        return 1;
    }

    const termchart::term::TerminalSize size = terminal.size();
    const std::uint32_t cols =
        size.valid() ? static_cast<std::uint32_t>(size.cols) : 80u;
    const std::uint32_t rows =
        (size.valid() && size.rows > 10u)
            ? static_cast<std::uint32_t>(size.rows) - 1u
            : 24u;

    const std::uint32_t header = 1u;
    const std::uint32_t body = rows - header;
    const std::uint32_t top_h = body * 3u / 5u;
    const std::uint32_t bot_h = body - top_h;
    const std::uint32_t left_w = cols / 2u;
    const std::uint32_t right_w = (cols > left_w + 1) ? cols - left_w - 1u : 0u;

    termchart::canvas::Canvas canvas(cols, rows);
    termchart::Style hs;
    hs.set_fg(termchart::Color::rgb(180, 180, 255)).set_attr(termchart::Attr::Bold);
    draw_text(canvas,
              "termchart dashboard :: line | bar | heatmap  (raster bypass, no chart API)",
              0, 0, hs);

    {
        termchart::canvas::Canvas p(left_w, top_h);
        draw_line_chart(p, left_w, top_h);
        blit_panel(p, canvas, 0u, header);
    }
    {
        termchart::canvas::Canvas p(right_w, top_h);
        draw_bar_chart(p, right_w, top_h);
        blit_panel(p, canvas, left_w + 1u, header);
    }
    {
        termchart::canvas::Canvas p(cols, bot_h);
        draw_heatmap(p, cols, bot_h);
        blit_panel(p, canvas, 0u, header + top_h);
    }

    termchart::render::Renderer renderer(terminal.caps().color_depth);
    // 位图出口（TERMCHART_PNG=out.png）：把终端画面原样存成 PNG。走库级的
    // gui::render_cells_to_bitmap（字符网格 → 位图）：盲文按 Unicode 点阵画、半块按
    // 字符语义画（半块热力图靠它表达数值）、其余走库内 5×7/TTF 字体。
    if (const char* png_path = std::getenv("TERMCHART_PNG");
        png_path != nullptr && *png_path != '\0') {
        const termchart::image::Bitmap bmp =
            termchart::gui::render_cells_to_bitmap(canvas.to_buffer(), termchart::gui::CellBitmapOptions{.cell_w = 6.0, .cell_h = 10.0});
        const std::string png = termchart::image::encode_png(bmp);
        std::ofstream out(png_path, std::ios::binary | std::ios::out);
        out.write(png.data(), static_cast<std::streamsize>(png.size()));
    }
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    if (termchart::api::write_stdout(bytes) != termchart::ErrorCode::Ok) return 1;
    if (termchart::api::write_stdout("\n") != termchart::ErrorCode::Ok) return 1;
    (void)termchart::api::flush_stdout();
    return terminal.leave() == termchart::ErrorCode::Ok ? 0 : 1;
}
