// 40_advanced_master.cpp —— termchart 高级能力旗舰展示
//
// 一整屏的"真·高级图"：把本库最能打的能力全部塞进同一个自适应终端画面里，
// 让你直观看到——同一个 Canvas 上可以同时堆叠出不同渲染物种的图：
//
//   ┌───────────────────────────────────────────────────────────┐
//   │  header：标题栏（加粗 + 真彩）                              │
//   ├───────────────┬───────────────────────┬───────────────────┤
//   │  ① 折线图      │  ② 渐变柱状图           │  ③ 真彩热力图       │
//   │  braille 子像素 │  Turbo 彩虹渐变         │  halfblock 上下双色 │
//   │  坐标轴+网格+图例│  （字符级渐进上亮）       │  （每字符 1×2 双色） │
//   ├───────────────┴───────────────────────┴───────────────────┤
//   │  ④ 思维导图（graph/mindmap 树布局 + 分支着色）                │
//   └───────────────────────────────────────────────────────────┘
//
// 技术要点（相对 03/04/05/06 的"进阶"）：
//   * 不写死画布，先用 terminal.size() 取真实终端尺寸再等比布局（避免折行/滚动错位）；
//   * 四种渲染物种混排：braille(2×4) / halfblock(1×2) / 块字符 / 文本，互不冲突；
//   * 真彩取色走 color::sample(Turbo) + 明暗调制，非 ANSI16 固定色；
//   * 子像素场浮点累积后一次性光栅化，曲线平滑；
//   * 末了把同一场景导出成 SVG，证明"一份数据多物种输出"（vector 后端）。
//
// 跑法（Windows，cmd）： chcp 65001 && build\ninja\examples\40_advanced_master.exe
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/backend/vector/svg_backend.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/color/colormap.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/diagrams/mindmap.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/media/image/field.hpp"
#include "termchart/layout/axis.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/layout.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/raster/rasterizer.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"
#include "termchart/scale/ticks.hpp"
#include "termchart/term/size.hpp"
#include "termchart/term/terminal.hpp"

namespace {

using namespace termchart;

constexpr char32_t kBrailleBlank = U'\u2800';   // 盲文熄灭格
constexpr char32_t kFullBlock = U'\u2588';      // █ 全实心块
constexpr char32_t kUpperHalf = U'\u2580';      // ▀ 上半块
constexpr char32_t kHLight = U'\u2500';         // ─
constexpr char32_t kVLight = U'\u2502';         // │
constexpr double kPi = 3.14159265358979323846;
constexpr double kXMax = kPi * 6.0;             // 数据 x 上限：三个周期

// ── Turbo 彩虹取色 + 明暗调制：比 ANSI16 更"高级"的真彩着色 ────────────────
Color colorize(double hue, double shade) {
    const Rgb base = color::sample(color::ColormapKind::Turbo, std::clamp(hue, 0.0, 1.0));
    const double b = 0.30 + 0.70 * std::clamp(shade, 0.0, 1.0);
    return Color::rgb(static_cast<std::uint8_t>(std::lround(base.r * b)),
                      static_cast<std::uint8_t>(std::lround(base.g * b)),
                      static_cast<std::uint8_t>(std::lround(base.b * b)));
}

// 在画布上画一行 ASCII 文本（纯工具，不依赖 document 层）。
void put_text(canvas::Canvas& cv, std::uint32_t row, std::uint32_t col,
              const char* s, const Style& st) {
    for (std::uint32_t i = 0; s[i] != '\0'; ++i) {
        cv.put(col + i, row, static_cast<char32_t>(s[i]), st);
    }
}

// 把一块子画布整体贴到主画布（空格也照拷，区域互不重叠）。
void blit_panel(const canvas::Canvas& src, canvas::Canvas& dst, std::uint32_t ox,
                std::uint32_t oy) {
    const CellBuffer buf = src.to_buffer();
    for (std::uint32_t y = 0; y < buf.height(); ++y) {
        for (std::uint32_t x = 0; x < buf.width(); ++x) {
            const Cell& c = buf.at(x, y);
            dst.put(ox + x, oy + y, c.cp, c.style);
        }
    }
}

// ── ① 折线图：braille 子像素平滑曲线 + 轴 + 网格 + 图例 ─────────────────────
// 原理：先在 (2w × 4h) 的子像素场里描点，再用 RasterKind::Braille 一次性光栅化，
// 曲线在高一档分辨率上走，转角圆润而不糊成块。
CellBuffer render_curve(const layout::PlotArea& area, double (*fn)(double),
                        double xmax) {
    const Rect inner = area.inner_rect();
    const std::uint32_t iw = inner.w > 1 ? static_cast<std::uint32_t>(inner.w) : 1u;
    const std::uint32_t ih = inner.h > 1 ? static_cast<std::uint32_t>(inner.h) : 1u;
    image::FloatField field(iw * 2u, ih * 4u);
    const std::uint32_t samples = std::max(8u, iw * 8u);
    for (std::uint32_t i = 0; i <= samples; ++i) {
        const double x = xmax * static_cast<double>(i) / static_cast<double>(samples);
        const double y = fn(x);
        const Point g = area.cell_of(x, y);
        const std::uint32_t sx =
            static_cast<std::uint32_t>(std::lround((g.x - inner.x) * 2.0));
        const std::uint32_t sy =
            static_cast<std::uint32_t>(std::lround((g.y - inner.y) * 4.0));
        if (sx < field.width() && sy < field.height()) {
            field.set(sx, sy, 1.0f);
        }
    }
    raster::RasterOptions opts;
    opts.fg = Color::ansi16(7);
    return raster::rasterize(field, raster::RasterKind::Braille, opts);
}

void draw_line_chart(canvas::Canvas& cv, std::uint32_t w, std::uint32_t h) {
    layout::PlotArea area(w, h, layout::Margin{}, 0.0, kXMax, -1.3, 1.3);
    const Rect inner = area.inner_rect();
    const std::vector<double> xt = scale::nice_ticks(0.0, kXMax);
    const std::vector<double> yt = scale::nice_ticks(-1.3, 1.3);

    Style axis;
    axis.set_fg(Color::ansi16(8));
    layout::draw_grid(cv, area, xt, yt, axis);        // 背景网格
    layout::draw_axis_x(cv, area, xt, "x", axis);
    layout::draw_axis_y(cv, area, yt, "y", axis);

    Style title;
    title.set_fg(Color::rgb(120, 220, 255)).set_attr(Attr::Bold);
    layout::draw_title(cv, "braille curves", inner, layout::HAlign::Center, title);

    // 三条正弦合成曲线，各自上色。
    const struct {
        double (*fn)(double);
        Color color;
        const char* name;
    } curves[] = {
        {[](double x) { return std::sin(x); }, Color::rgb(86, 197, 154), "sin"},
        {[](double x) { return std::cos(x); }, Color::rgb(255, 150, 90), "cos"},
        {[](double x) { return 0.5 * std::sin(x * 2.0); }, Color::rgb(180, 120, 255), "sin2"},
    };

    const std::uint32_t ox = inner.x > 0 ? static_cast<std::uint32_t>(inner.x) : 0u;
    const std::uint32_t oy = inner.y > 0 ? static_cast<std::uint32_t>(inner.y) : 0u;
    std::uint32_t leg_x = ox;
    for (const auto& c : curves) {
        const CellBuffer wave = render_curve(area, c.fn, kXMax);
        // 把盲文光栅结果按各曲线颜色落到画布。
        for (std::uint32_t cy = 0; cy < wave.height(); ++cy) {
            for (std::uint32_t cx = 0; cx < wave.width(); ++cx) {
                const Cell& cc = wave.at(cx, cy);
                if (cc.cp == kBrailleBlank) continue;
                Style s;
                s.set_fg(c.color);
                cv.put(ox + cx, oy + cy, cc.cp, s);
            }
        }
        // 图例文字：左上角堆叠。
        Style ls;
        ls.set_fg(c.color).set_attr(Attr::Bold);
        put_text(cv, 1u, leg_x, c.name, ls);
        leg_x += static_cast<std::uint32_t>(std::strlen(c.name)) + 2u;
    }
}

// ── ② 渐变柱状图：Turbo 彩虹渐变，柱体自下而上渐亮 ────────────────────────
void draw_bar_chart(canvas::Canvas& cv, std::uint32_t w, std::uint32_t h) {
    constexpr std::uint32_t N = 8;
    const double data[N] = {0.30, 0.85, 0.55, 0.92, 0.40, 0.70, 0.62, 0.48};
    const std::uint32_t axis_x = 2u;
    const std::uint32_t baseline = (h > 3u) ? h - 2u : h - 1u;
    double max_v = 0.0;
    for (std::uint32_t i = 0; i < N; ++i) {
        max_v = std::max(max_v, data[i]);
    }

    Style ax;
    ax.set_fg(Color::ansi16(8));
    for (std::uint32_t y = 2u; y <= baseline && y < h; ++y) {
        cv.put(axis_x, y, kVLight, ax);
    }
    for (std::uint32_t x = axis_x; x < w; ++x) {
        cv.put(x, baseline, kHLight, ax);
    }

    Style t;
    t.set_fg(Color::rgb(255, 200, 120)).set_attr(Attr::Bold);
    layout::draw_title(cv, "gradient bars",
                       Rect{0.0, 0.0, static_cast<double>(w), static_cast<double>(h)},
                       layout::HAlign::Center, t);

    const std::uint32_t bar_area = (w > axis_x + 1u) ? w - (axis_x + 1u) : 0u;
    const std::uint32_t bar_w = bar_area / N;
    const std::uint32_t usable_h = (baseline > 2u) ? baseline - 2u : 0u;
    for (std::uint32_t i = 0; i < N; ++i) {
        const double fh = (data[i] / max_v) * static_cast<double>(usable_h);
        const std::uint32_t full = static_cast<std::uint32_t>(std::floor(fh));
        const std::uint32_t bx0 = axis_x + 1u + i * bar_w;
        const double hue = static_cast<double>(i) / static_cast<double>(N - 1u);
        // 柱体自下而上渐亮，颜色仍是该柱的彩虹色相。
        for (std::uint32_t r = 0; r < full && r < usable_h; ++r) {
            const double shade =
                0.35 + 0.65 * (static_cast<double>(r) / static_cast<double>(std::max(1u, usable_h)));
            const Color col = colorize(hue, shade);
            Style s;
            s.set_fg(col);
            for (std::uint32_t x = bx0; x < bx0 + bar_w && x < w; ++x) {
                cv.put(x, baseline - 1u - r, kFullBlock, s);
            }
        }
        if (full < usable_h) {
            Style s;
            s.set_fg(colorize(hue, 0.35));
            for (std::uint32_t x = bx0; x < bx0 + bar_w && x < w; ++x) {
                cv.put(x, baseline - 1u - full, kFullBlock, s);
            }
        }
        Style ls;
        ls.set_fg(Color::ansi16(7));
        put_text(cv, baseline + 1u, bx0, std::to_string(i + 1).c_str(), ls);
    }
}

// ── ③ 真彩热力图：halfblock 每字符上下两色，纵向分辨率 ×2 的真彩 ────────────
double ripple(double u, double v) {
    const double dx = u - 0.5;
    const double dy = v - 0.5;
    const double d = std::sqrt(dx * dx + dy * dy);
    return 0.5 + 0.5 * std::sin(14.0 * d) * std::cos(4.0 * std::atan2(dy, dx));
}

void draw_heatmap(canvas::Canvas& cv, std::uint32_t w, std::uint32_t h) {
    Style t;
    t.set_fg(Color::rgb(180, 255, 180)).set_attr(Attr::Bold);
    layout::draw_title(cv, "true-color heatmap",
                       Rect{0.0, 0.0, static_cast<double>(w), static_cast<double>(h)},
                       layout::HAlign::Center, t);
    if (h <= 2u) return;
    const double denom = static_cast<double>(2 * (h - 1));
    for (std::uint32_t y = 1u; y < h; ++y) {
        for (std::uint32_t x = 0u; x < w; ++x) {
            const double u =
                (w > 1u) ? static_cast<double>(x) / static_cast<double>(w - 1u) : 0.5;
            const double vt = static_cast<double>(y * 2u) / denom;
            const double vb = static_cast<double>(y * 2u + 1u) / denom;
            const Color ct = colorize(ripple(u, vt), 1.0);
            const Color cb = colorize(ripple(u, vb), 1.0);
            Style s;
            s.set_fg(ct).set_bg(cb);
            cv.put(x, y, kUpperHalf, s);  // ▀：上=fg 下=bg，一字符两色
        }
    }
}

// ── ④ 思维导图：graph/mindmap 树布局 + 分支着色 ────────────────────────────
void draw_mindmap(canvas::Canvas& cv, std::uint32_t w, std::uint32_t h) {
    graph::Graph graph;
    const graph::NodeId center = graph.add_node("termchart").id();
    const graph::NodeId render = graph.add_node("render").id();
    const graph::NodeId raster = graph.add_node("raster").id();
    const graph::NodeId chart = graph.add_node("chart").id();
    const graph::NodeId graph_mod = graph.add_node("graph").id();
    const graph::NodeId backend = graph.add_node("backend").id();
    const graph::NodeId term = graph.add_node("term").id();
    (void)graph.add_edge(center, render);
    (void)graph.add_edge(center, raster);
    (void)graph.add_edge(center, chart);
    (void)graph.add_edge(center, graph_mod);
    (void)graph.add_edge(center, backend);
    (void)graph.add_edge(center, term);
    (void)graph.add_edge(render, raster);

    graph::MindmapOptions opts;
    opts.topic_style = Style{}.set_fg(Color::rgb(255, 120, 200)).set_attr(Attr::Bold);
    opts.branch_style = Style{}.set_fg(Color::rgb(120, 220, 255));
    (void)graph::render_mindmap(graph, cv, opts);
}

}  // namespace

int main() {
    term::Terminal terminal(term::TerminalOptions{
        .alt_screen = false, .hide_cursor = false});
    if (terminal.enter() != ErrorCode::Ok) {
        std::fprintf(stderr, "无法进入终端模式\n");
        return 1;
    }

    // 自适应：不写死尺寸，优先读真实终端大小；被重定向/过小时回退到默认。
    const term::TerminalSize size = terminal.size();
    const std::uint32_t cols =
        (size.valid() && size.cols > 20u) ? static_cast<std::uint32_t>(size.cols) : 80u;
    const std::uint32_t rows =
        (size.valid() && size.rows > 12u) ? static_cast<std::uint32_t>(size.rows) - 1u : 28u;

    const std::uint32_t header = 1u;
    const std::uint32_t body = (rows > header) ? rows - header : 8u;
    const std::uint32_t top_h = static_cast<std::uint32_t>(body * 3u / 5u);
    const std::uint32_t bot_h = (body > top_h) ? body - top_h : 8u;
    const std::uint32_t three_w = std::max(12u, cols / 3u);
    const std::uint32_t right_w = (cols > three_w * 2u) ? cols - three_w * 2u : three_w;

    canvas::Canvas canvas(cols, rows);

    // ① 折线图（左）
    {
        canvas::Canvas p(three_w, top_h);
        draw_line_chart(p, three_w, top_h);
        blit_panel(p, canvas, 0u, header);
    }
    // ② 渐变柱状图（中）
    {
        canvas::Canvas p(three_w, top_h);
        draw_bar_chart(p, three_w, top_h);
        blit_panel(p, canvas, three_w, header);
    }
    // ③ 真彩热力图（右）
    {
        canvas::Canvas p(right_w, top_h);
        draw_heatmap(p, right_w, top_h);
        blit_panel(p, canvas, three_w * 2u, header);
    }
    // ④ 思维导图（下半屏，全宽）
    {
        canvas::Canvas p(cols, bot_h);
        draw_mindmap(p, cols, bot_h);
        blit_panel(p, canvas, 0u, header + top_h);
    }

    render::Renderer renderer(terminal.caps().color_depth);
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    if (api::write_stdout(bytes) != ErrorCode::Ok) return 1;
    if (api::write_stdout("\n") != ErrorCode::Ok) return 1;
    (void)api::flush_stdout();

    // 附赠：把同一份"数据故事"导出成 SVG，演示 vector 后端（一份数据多物种输出）。
    scene::Scene scene;
    const double w = 60.0, h = 20.0;
    scene.push(scene::StrokeOp{Color::ansi16(8), 1.0});
    scene.push(scene::RectOp{Rect{0.0, 0.0, w, h}});
    scene.push(scene::StrokeOp{Color::rgb(86, 197, 154), 1.5});
    scene.push(scene::MoveTo{Point{0.0, h / 2.0}});
    for (int i = 0; i <= 120; ++i) {
        const double x = w * static_cast<double>(i) / 120.0;
        const double y = h / 2.0 - (h / 2.0 - 1.0) * std::sin(x / 6.0);
        scene.push(scene::LineTo{Point{x, y}});
    }
    scene.push(scene::TextOp{Point{1.0, 1.0}, "sin(x)",
                             Style{}.set_fg(Color::rgb(86, 197, 154))});
    backend::vector::SvgBackend svg;
    svg.set_options(backend::vector::VectorOptions{.scale = 10.0});
    const std::string rendered = backend::vector::render_vector(svg, scene);
    api::write_stdout("\n-- svg export (" + std::to_string(rendered.size()) +
                      " bytes) --\n");
    api::write_stdout(rendered);

    return terminal.leave() == ErrorCode::Ok ? 0 : 1;
}