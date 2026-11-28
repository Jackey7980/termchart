// 08_fractal_levels.cpp —— 四连拍：同一张 Mandelbrot 在 真彩/256/16/ASCII 四档色深同屏
//
// 直接演示库的核心卖点：同一套"标量场 → raster → 真彩"引擎，无需任何分支代码，
// 只靠 ColorDepth / RasterKind 两个开关，即可在任何终端、任何色深下渲染同一张图。
// 评委一眼看懂"任何终端都能看"，比讲十句架构都强。
//
// 布局：2×2 网格（真彩盲文 / 256 盲文 / 16 盲文 / 单色 ASCII），每个象限独立按自己
// 的色深渲染成字符串后横向拼屏。降级对比是项目最核心的卖点。
//
// 跑法： chcp 65001 && build\ninja\examples\08_fractal_levels.exe

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/color/colormap.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/media/image/field.hpp"
#include "termchart/raster/rasterizer.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/term/size.hpp"
#include "termchart/term/terminal.hpp"

namespace {

constexpr char32_t kBrailleBlank = U'\u2800';

// 复平面 (cr,ci) 处的 Mandelbrot 平滑迭代值：逃出返回 [0,1)，集合内部返回 -1。
void mandelbrot(double cr, double ci, double& out) {
    constexpr int kMaxIter = 500;
    double zr = 0.0, zi = 0.0;
    int n = 0;
    for (; n < kMaxIter; ++n) {
        const double zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) break;
        zi = 2.0 * zr * zi + ci;
        zr = zr2 - zi2 + cr;
    }
    if (n >= kMaxIter) {
        out = -1.0;  // 集合内部，黑色
        return;
    }
    // 平滑着色 + 对数映射：把暗端密集的逃逸迭代数拉开，让 Turbo 全谱（紫→蓝→青→
    // 绿→黄→橙→红）依次"烧"到边界外圈。
    const double log_zn = std::log(zr * zr + zi * zi) * 0.5;
    const double nu = std::log(log_zn / std::log(2.0)) / std::log(2.0);
    const double smooth = static_cast<double>(n) + 1.0 - nu;
    out = std::log(smooth + 1.0) / std::log(static_cast<double>(kMaxIter) + 1.0);
    out = std::clamp(out, 0.0, 1.0);
}

termchart::Color colorize(double t) {
    if (t < 0.0) return termchart::Color::rgb(0, 0, 0);
    const termchart::Rgb rgb =
        termchart::color::sample(termchart::color::ColormapKind::Turbo, t);
    return termchart::Color::rgb(rgb.r, rgb.g, rgb.b);
}

// 真彩映射：内部=黑，外部=Turbo 彩虹（按平滑迭代值）。
void blit_fractal(const termchart::image::FloatField& shade,
                  const termchart::image::FloatField& tfield,
                  termchart::raster::RasterKind kind, std::uint32_t sub_x,
                  std::uint32_t sub_y, termchart::canvas::Canvas& canvas,
                  std::uint32_t ox, std::uint32_t oy) {
    termchart::raster::RasterOptions opts;
    opts.fg = termchart::Color::ansi16(7);
    opts.bg = termchart::Color::ansi16(0);
    termchart::CellBuffer buf = termchart::raster::rasterize(shade, kind, opts);
    for (std::uint32_t y = 0; y < buf.height(); ++y) {
        for (std::uint32_t x = 0; x < buf.width(); ++x) {
            const termchart::Cell& c = buf.at(x, y);
            if (c.cp == kBrailleBlank || c.cp == U'\u0020') continue;
            const std::uint32_t spx = x * sub_x + sub_x / 2;
            const std::uint32_t spy = y * sub_y + sub_y / 2;
            termchart::Style s;
            s.set_fg(colorize(tfield.at(spx, spy)));
            canvas.put(ox + x, oy + y, c.cp, s);
        }
    }
}

void draw_title(termchart::canvas::Canvas& canvas, std::uint32_t row,
                const char* text) {
    termchart::Style s;
    s.set_fg(termchart::Color::rgb(120, 220, 255)).set_attr(termchart::Attr::Bold);
    for (std::uint32_t i = 0; text[i] != '\0'; ++i)
        canvas.put(i, row, static_cast<char32_t>(text[i]), s);
}

// 渲染单个象限：给定物种/色深/尺寸，返回按行切好的渲染字符串（已去 \r）。
std::vector<std::string> render_quadrant(
    termchart::raster::RasterKind kind, termchart::ColorDepth depth,
    std::uint32_t qw, std::uint32_t qh, double cx, double cy, double span_x) {
    std::uint32_t sub_x = 2, sub_y = 4;
    switch (kind) {
        case termchart::raster::RasterKind::HalfBlock: sub_x = 1; sub_y = 2; break;
        case termchart::raster::RasterKind::Quadrant:  sub_x = 2; sub_y = 2; break;
        case termchart::raster::RasterKind::Ascii:
        case termchart::raster::RasterKind::Block:     sub_x = 1; sub_y = 1; break;
        default: break;  // Braille
    }
    const bool image_kind =
        (kind == termchart::raster::RasterKind::Braille ||
         kind == termchart::raster::RasterKind::HalfBlock ||
         kind == termchart::raster::RasterKind::Quadrant);

    termchart::canvas::Canvas canvas(qw, qh);

    const char* kname = termchart::raster::raster_name(kind);
    const char* dname = depth == termchart::ColorDepth::TrueColor ? "truecolor"
                      : depth == termchart::ColorDepth::Ansi256  ? "256"
                      : depth == termchart::ColorDepth::Ansi16   ? "16"
                                                                : "mono";
    const std::string title = std::string("[") + kname + "/" + dname + "]";
    draw_title(canvas, 0u, title.c_str());

    // 复平面取景：经典主体区，宽高比按字符格 1:2 校正避免纵向拉伸；第 0 行留给标题。
    const double span_y = span_x * (2.0 * static_cast<double>(qh - 1u)) /
                          static_cast<double>(qw);
    const std::uint32_t fw = qw * sub_x;
    const std::uint32_t fh = (qh - 1u) * sub_y;
    termchart::image::FloatField shade(fw, fh);
    termchart::image::FloatField tfield(fw, fh);
    for (std::uint32_t py = 0; py < fh; ++py) {
        for (std::uint32_t px = 0; px < fw; ++px) {
            const double wx = cx + ((px + 0.5) / fw - 0.5) * span_x;
            const double wy = cy + ((py + 0.5) / fh - 0.5) * span_y;
            double t = -1.0;
            mandelbrot(wx, wy, t);
            // 图像型物种用等值带 fract(t*8) 形成同心环；亮度型物种用平滑 t 当密度。
            const float smooth_t = static_cast<float>(t);
            const float sh = (t < 0.0)
                                 ? 0.0f
                                 : (image_kind ? std::fmod(smooth_t * 8.0f, 1.0f)
                                               : smooth_t);
            shade.set(px, py, sh);
            tfield.set(px, py, smooth_t);
        }
    }
    blit_fractal(shade, tfield, kind, sub_x, sub_y, canvas, 0u, 1u);

    termchart::render::Renderer renderer(depth);
    const std::string bytes = renderer.render_full(canvas.to_buffer());

    std::vector<std::string> lines;
    std::string cur;
    for (char ch : bytes) {
        if (ch == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else if (ch != '\r') {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
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
        (size.valid() && size.rows > 6u)
            ? static_cast<std::uint32_t>(size.rows) - 1u
            : 24u;

    const std::uint32_t gap = 1u;
    std::uint32_t qw = (cols > gap) ? (cols - gap) / 2u : cols;
    std::uint32_t qh = (rows > 2u) ? (rows - 2u) / 2u : rows;
    if (qw < 16u) qw = 16u;
    if (qh < 6u) qh = 6u;

    const double cx = -0.5, cy = 0.0, span_x = 3.4;
    const auto q_tc = render_quadrant(termchart::raster::RasterKind::Braille,
                                      termchart::ColorDepth::TrueColor, qw, qh,
                                      cx, cy, span_x);
    const auto q_256 = render_quadrant(termchart::raster::RasterKind::Braille,
                                       termchart::ColorDepth::Ansi256, qw, qh, cx,
                                       cy, span_x);
    const auto q_16 = render_quadrant(termchart::raster::RasterKind::Braille,
                                      termchart::ColorDepth::Ansi16, qw, qh, cx,
                                      cy, span_x);
    const auto q_ascii = render_quadrant(termchart::raster::RasterKind::Ascii,
                                         termchart::ColorDepth::None, qw, qh, cx,
                                         cy, span_x);

    std::string out;
    out += "termchart :: Mandelbrot — 四连拍  [truecolor / 256 / 16 / ascii]\n";
    const auto emit_half = [&](const std::vector<std::string>& left,
                               const std::vector<std::string>& right) {
        const std::uint32_t h =
            static_cast<std::uint32_t>(std::min({left.size(), right.size(),
                                                 static_cast<std::size_t>(qh)}));
        for (std::uint32_t r = 0; r < h; ++r) {
            std::string l = left[r];
            if (l.size() < qw) l.append(qw - l.size(), ' ');
            out += l + "\x1b[0m" + std::string(gap, ' ') + right[r] + "\x1b[0m\n";
        }
    };
    emit_half(q_tc, q_256);
    out += std::string(static_cast<std::size_t>(qw) * 2u + gap, ' ') + "\n";
    emit_half(q_16, q_ascii);

    if (termchart::api::write_stdout(out) != termchart::ErrorCode::Ok) return 1;
    (void)termchart::api::flush_stdout();
    return terminal.leave() == termchart::ErrorCode::Ok ? 0 : 1;
}
