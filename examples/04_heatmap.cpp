// 04_heatmap.cpp —— 连续色阶热力图（Mandelbrot，viridis 色表 + 半块双纵向像素）
//
// 展示 image::FloatField 作为 2D 强度场 + color/colormap 连续色表的组合：
// 每个字符用半块 ▀，上半取 colormap(上子像素)、下半取 colormap(下子像素)，
// 于是一个字符格承载纵向 2 个独立着色的子像素 —— 纵向分辨率 ×2 且全彩色。
// 之所以不直接用 raster/halfblock：它的 fg/bg 只有两种固定二值色，
// 连续色阶热力图必须由 colormap 逐格取色 + 半块字符自行组合。
//
// **尺寸自适应终端**：图像必须严格等于终端可视区（宽 = cols，高 = rows - 1），
// 否则超宽会每行折行、超高会滚动，两者都会让图像错位（看起来像"颜色溢出"）。
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/color/colormap.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/term/size.hpp"
#include "termchart/term/terminal.hpp"

namespace {

constexpr int kMaxIter = 120;

// 逃逸迭代次数 → [0,1] 强度（集合内为 0 → 深紫，外部越浅越亮 → 黄）。
double mandel_escape(double cx, double cy) noexcept {
    double zx = 0.0;
    double zy = 0.0;
    for (int i = 0; i < kMaxIter; ++i) {
        const double xt = zx * zx - zy * zy + cx;
        zy = 2.0 * zx * zy + cy;
        zx = xt;
        if (zx * zx + zy * zy > 4.0) {
            return 1.0 - static_cast<double>(i) / static_cast<double>(kMaxIter);
        }
    }
    return 0.0;
}

termchart::Color viridis(double t) noexcept {
    const termchart::Rgb c =
        termchart::color::sample(termchart::color::ColormapKind::Viridis, t);
    return termchart::Color::rgb(c.r, c.g, c.b);
}

}  // namespace

int main() {
    termchart::term::Terminal terminal(termchart::term::TerminalOptions{
        .alt_screen = false, .hide_cursor = false});
    if (terminal.enter() != termchart::ErrorCode::Ok) {
        std::fprintf(stderr, "无法进入终端模式\n");
        return 1;
    }

    // 自适应终端可视区：宽度占满，高度留 1 行给返回后的命令行提示符。
    const termchart::term::TerminalSize size = terminal.size();
    const std::uint32_t cols =
        size.valid() ? static_cast<std::uint32_t>(size.cols) : 80u;
    const std::uint32_t rows =
        (size.valid() && size.rows > 6u) ? static_cast<std::uint32_t>(size.rows) - 1u : 24u;

    // 等比取景：终端字符格近似 1:2（宽:高），像素宽高比 = cols : rows*2。
    // 取 span_y = span_x * cols / (2*rows)，使 Mandelbrot 的圆形在终端里
    // 仍是圆形（否则会被拉成椭圆）。
    constexpr double kCx0 = -2.2;
    constexpr double kSpanX = 3.0;
    const double span_x = kSpanX;
    const double span_y =
        span_x * (static_cast<double>(cols) / (2.0 * static_cast<double>(rows)));
    const double cy0 = -span_y * 0.5;
    const double field_rows = 2.0 * static_cast<double>(rows);

    termchart::canvas::Canvas canvas(cols, rows);
    for (std::uint32_t x = 0; x < cols; ++x) {
        const double cx =
            kCx0 + span_x * static_cast<double>(x) / static_cast<double>(cols - 1);
        for (std::uint32_t y = 0; y < rows; ++y) {
            const double cy_top =
                cy0 + span_y * (2.0 * static_cast<double>(y)) / field_rows;
            const double cy_bot =
                cy0 + span_y * (2.0 * static_cast<double>(y) + 1.0) / field_rows;
            termchart::Style s;
            s.set_fg(viridis(mandel_escape(cx, cy_top)))
                .set_bg(viridis(mandel_escape(cx, cy_bot)));
            canvas.put(x, y, U'\u2580', s);  // ▀ 上半前景 / 下半背景
        }
    }

    termchart::render::Renderer renderer(terminal.caps().color_depth);
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    if (termchart::api::write_stdout(bytes) != termchart::ErrorCode::Ok) return 1;
    if (termchart::api::write_stdout("\n") != termchart::ErrorCode::Ok) return 1;
    (void)termchart::api::flush_stdout();
    return terminal.leave() == termchart::ErrorCode::Ok ? 0 : 1;
}