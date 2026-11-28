// 76_csv_to_chart.cpp —— 数据管线端到端：CSV → 清洗 → 均线 → 终端图
//
// 把 io / data / math 三个数据模块缝进真实出图链路（数据帝国对用户的最终兑现）：
//   ① io/csv：零拷贝解析内置 CSV 文本（含 NA 缺失值）；
//   ② data/fill_missing：LinearInterp 清洗（NaN → 相邻插值）；
//   ③ data/rolling：ema(window=5) 算指数移动平均线；
//   ④ math/stats：pearson/mean 等统计量；
//   ⑤ 绘制栈与 03_function_plot 同源：layout 原语（轴/网格/图例/标题）+
//      FloatField + braille 2×4 光栅 + Canvas + Renderer → ANSI。
// 数据与算法全部确定性：同一构建永远同一张图（ctest 可跑）。
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/data/fill_missing.hpp"
#include "termchart/data/rolling.hpp"
#include "termchart/media/image/field.hpp"
#include "termchart/io/csv.hpp"
#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/layout.hpp"
#include "termchart/layout/legend.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/math/stats.hpp"
#include "termchart/raster/rasterizer.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/scale/ticks.hpp"
#include "termchart/term/terminal.hpp"

namespace {

constexpr std::uint32_t kCanvasW = 80;
constexpr std::uint32_t kCanvasH = 26;
constexpr char32_t kBrailleBlank = U'\u2800';
constexpr std::size_t kEmaWindow = 5;

// 确定性样本：带 3 个 NA 缺失的带噪序列（禁随机）。
constexpr const char* kCsv =
    "t,v\n"
    "0,10.2\n1,11.8\n2,NA\n3,13.1\n4,12.6\n5,14.9\n6,NA\n7,15.3\n"
    "8,14.1\n9,13.5\n10,15.8\n11,17.2\n12,16.4\n13,NA\n14,18.1\n"
    "15,19.6\n16,18.9\n17,17.7\n18,20.3\n19,21.5\n20,20.8\n";

constexpr termchart::Rgb kRawColor{86, 197, 154};    // 原始序列：青绿
constexpr termchart::Rgb kEmaColor{246, 193, 90};    // 均线：琥珀

// 把光栅化出的曲线层叠加到画布 inner 区域（仅覆盖点亮的子像素）。
void blit(const termchart::CellBuffer& wave, termchart::canvas::Canvas& canvas,
          const termchart::Rect& inner) {
    const auto ox = static_cast<std::uint32_t>(inner.x);
    const auto oy = static_cast<std::uint32_t>(inner.y);
    for (std::uint32_t cy = 0; cy < wave.height(); ++cy) {
        for (std::uint32_t cx = 0; cx < wave.width(); ++cx) {
            const termchart::Cell& c = wave.at(cx, cy);
            if (c.cp != kBrailleBlank) {
                canvas.put(ox + cx, oy + cy, c.cp, c.style);
            }
        }
    }
}

// 离散序列 → FloatField → braille 光栅（与 03 的 render_curve 同构，数据驱动）。
termchart::CellBuffer render_series(const termchart::layout::PlotArea& area,
                                     const std::vector<double>& xs,
                                     const std::vector<double>& ys, termchart::Rgb color) {
    const termchart::Rect inner = area.inner_rect();
    const auto inner_w = static_cast<std::uint32_t>(inner.w);
    const auto inner_h = static_cast<std::uint32_t>(inner.h);
    termchart::image::FloatField field(inner_w * 2u, inner_h * 4u);
    for (std::size_t i = 0; i < xs.size() && i < ys.size(); ++i) {
        if (std::isnan(ys[i])) {
            continue;
        }
        const termchart::Point g = area.cell_of(xs[i], ys[i]);
        const auto sx = static_cast<std::uint32_t>(std::lround((g.x - inner.x) * 2.0));
        const auto sy = static_cast<std::uint32_t>(std::lround((g.y - inner.y) * 4.0));
        if (sx < field.width() && sy < field.height()) {
            field.set(sx, sy, 1.0f);
        }
    }
    termchart::raster::RasterOptions opts;
    opts.fg = termchart::Color::rgb(color);
    return termchart::raster::rasterize(field, termchart::raster::RasterKind::Braille, opts);
}

}  // namespace

int main() {
    using namespace termchart;

    // ── ① io/csv ──────────────────────────────────────────────────────────
    io::ParseDiagnostic diag;
    const auto table = io::parse_csv(kCsv, io::CsvOptions{',', true, 0}, &diag);
    if (!table.has_value()) {
        std::fprintf(stderr, "CSV 解析失败：%s (line=%zu col=%zu)\n",
                     to_string(diag.code), diag.line, diag.column);
        return 1;
    }
    if (table->columns.size() < 2 || table->columns[1].empty()) {
        std::fprintf(stderr, "CSV 需要至少两列\n");
        return 1;
    }
    std::vector<double> ys = table->columns[1];
    const std::size_t n = ys.size();
    std::size_t missing = 0;
    for (const double v : ys) {
        if (std::isnan(v)) {
            ++missing;
        }
    }
    std::vector<double> xs(n);
    for (std::size_t i = 0; i < n; ++i) {
        xs[i] = static_cast<double>(i);
    }

    // ── ② data/fill_missing ───────────────────────────────────────────────
    (void)data::fill_missing(ys, data::FillStrategy::LinearInterp);

    // ── ③ data/rolling：EMA ───────────────────────────────────────────────
    const std::vector<double> ema = data::ema(ys, kEmaWindow);

    // ── ④ math/stats ──────────────────────────────────────────────────────
    const double mean = math::mean(std::span<const double>{ys.data(), ys.size()});
    const double r = math::pearson(std::span<const double>{xs.data(), n},
                                   std::span<const double>{ys.data(), n});

    // ── ⑤ 绘制栈（layout 原语 + braille 光栅，与 03 同源）─────────────────
    double y_lo = ys.front();
    double y_hi = ys.front();
    for (const double v : ys) {
        y_lo = std::min(y_lo, v);
        y_hi = std::max(y_hi, v);
    }
    const double pad = std::max(1.0, (y_hi - y_lo) * 0.1);

    term::Terminal terminal(term::TerminalOptions{.alt_screen = false, .hide_cursor = false});
    if (terminal.enter() != ErrorCode::Ok) {
        std::fprintf(stderr, "无法进入终端模式\n");
        return 1;
    }

    layout::PlotArea area(kCanvasW, kCanvasH, layout::Margin{}, 0.0,
                          static_cast<double>(n - 1), y_lo - pad, y_hi + pad);
    canvas::Canvas canvas(kCanvasW, kCanvasH);
    const Rect inner = area.inner_rect();

    const std::vector<double> xticks = scale::nice_ticks(0.0, static_cast<double>(n - 1));
    const std::vector<double> yticks = scale::nice_ticks(y_lo - pad, y_hi + pad);

    Style axis;
    axis.set_fg(Color::ansi16(8));
    layout::draw_grid(canvas, area, xticks, yticks, axis);
    layout::draw_axis_x(canvas, area, xticks, "t", axis);
    layout::draw_axis_y(canvas, area, yticks, "v", axis);

    Style title;
    title.set_fg(Color::rgb(143, 165, 190)).set_attr(Attr::Bold);
    layout::draw_title(canvas, "CSV -> fill_missing -> EMA(5) -> chart",
                       Rect{0.0, 0.0, static_cast<double>(kCanvasW), 1.0},
                       layout::HAlign::Left, title);

    const std::vector<layout::LegendEntry> legend = {
        {"raw", Color::rgb(kRawColor)},
        {"ema(5)", Color::rgb(kEmaColor)},
    };
    layout::draw_legend(
        canvas,
        std::span<const layout::LegendEntry>{legend.data(), legend.size()},
        Rect{46.0, 0.0, 30.0, 1.0}, layout::HAlign::Right);

    blit(render_series(area, xs, ys, kRawColor), canvas, inner);
    blit(render_series(area, xs, ema, kEmaColor), canvas, inner);

    render::Renderer renderer(terminal.caps().color_depth);
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    if (api::write_stdout(bytes) != ErrorCode::Ok) {
        (void)terminal.leave();
        return 1;
    }
    // 尾部统计行：数据管线的产物（mean/pearson/缺失数），ASCII 走终端栈。
    char tail[160] = {};
    std::snprintf(tail, sizeof(tail),
                  "\nn=%zu missing=%zu (LinearInterp) | mean=%.3f pearson(t,v)=%.3f | "
                  "ema(5) last=%.3f\n",
                  n, missing, mean, r, ema.empty() ? 0.0 : ema.back());
    if (api::write_stdout(tail) != ErrorCode::Ok) {
        (void)terminal.leave();
        return 1;
    }
    (void)api::flush_stdout();
    return terminal.leave() == ErrorCode::Ok ? 0 : 1;
}
