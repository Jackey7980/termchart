// 07_fractal.cpp —— 子像素光栅 + 真彩：Mandelbrot 分形（含降级对比开关）
//
// 与 05（3D 球体）、06（折线/柱状/热力仪表盘）不同维度：这是一张**连续真彩图像**。
// 它把"离散标量场 → 取字符（raster）→ 真彩（colormap）→ 终端"这条链路推到照片级密度。
//
// 两个开关用于产出"降级对比"——这是项目核心卖点（任何终端、任何色深都能看）：
//   --depth <true|256|16|mono>   强制输出色深（默认 truecolor）
//   --raster <braille|halfblock|quadrant|ascii|block>  选光栅物种
// 例： 07_fractal --depth 256          → 256 色盲文
//      07_fractal --raster ascii --depth mono   → 单色 ASCII 密度图
//
// 重要：默认 depth 用 TrueColor 而非终端探测值。这样 `./07_fractal > out.txt`
// 重定向时不会因非 tty 被 daemon_safe 降级成单色，文件内容与真终端完全一致。
//
// 子像素在屏幕上各向同性（每 subpixel ≈ 0.5 屏宽 × 0.5 屏高），但字符格高约是宽的 2 倍，
// 故世界坐标的 y 跨度按 (2·rows)/cols 做宽高比校正，避免纵向拉伸。
//
// 跑法（Windows 终端，cmd）： chcp 65001 && build\ninja\examples\07_fractal.exe
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
#include "termchart/image/field.hpp"
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
    // 平滑着色：消除整数迭代带来的色带
    const double log_zn = std::log(zr * zr + zi * zi) * 0.5;
    const double nu = std::log(log_zn / std::log(2.0)) / std::log(2.0);
    const double smooth = static_cast<double>(n) + 1.0 - nu;
    // 对数映射：逃逸迭代数在暗端高度密集，线性映射会让 Turbo 只露出紫→蓝；
    // 取 log 把密集区拉开，让青/绿/黄/红/白依次"烧"到边界外圈。
    out = std::log(smooth + 1.0) / std::log(static_cast<double>(kMaxIter) + 1.0);
    out = std::clamp(out, 0.0, 1.0);
}

// 真彩映射：内部=黑，外部=Turbo 彩虹（按平滑迭代值）。
termchart::Color colorize(double t) {
    if (t < 0.0) return termchart::Color::rgb(0, 0, 0);
    const termchart::Rgb rgb = termchart::color::sample(
        termchart::color::ColormapKind::Turbo, t);
    return termchart::Color::rgb(rgb.r, rgb.g, rgb.b);
}

// 全屏覆盖：shade 全 1（每格点亮），逐格按中心子像素的迭代值上真彩。
// sub_x/sub_y 为该物种子像素密度，用于从场回采样中心子像素。
void blit_fractal(const termchart::image::FloatField& shade,
                  const termchart::image::FloatField& tfield,
                  termchart::raster::RasterKind kind, std::uint32_t sub_x,
                  std::uint32_t sub_y, termchart::canvas::Canvas& canvas,
                  std::uint32_t ox, std::uint32_t oy) {
    termchart::raster::RasterOptions opts;
    opts.fg = termchart::Color::ansi16(7);
    opts.bg = termchart::Color::ansi16(0);
    termchart::CellBuffer buf =
        termchart::raster::rasterize(shade, kind, opts);
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

void draw_title(termchart::canvas::Canvas& canvas, const char* text) {
    termchart::Style s;
    s.set_fg(termchart::Color::rgb(120, 220, 255)).set_attr(termchart::Attr::Bold);
    for (std::uint32_t i = 0; text[i] != '\0'; ++i) {
        canvas.put(i, 0, static_cast<char32_t>(text[i]), s);
    }
}

termchart::raster::RasterKind parse_kind(const char* v) {
    if (std::strcmp(v, "halfblock") == 0)
        return termchart::raster::RasterKind::HalfBlock;
    if (std::strcmp(v, "quadrant") == 0)
        return termchart::raster::RasterKind::Quadrant;
    if (std::strcmp(v, "ascii") == 0)
        return termchart::raster::RasterKind::Ascii;
    if (std::strcmp(v, "block") == 0)
        return termchart::raster::RasterKind::Block;
    return termchart::raster::RasterKind::Braille;
}

termchart::ColorDepth parse_depth(const char* v) {
    if (std::strcmp(v, "256") == 0) return termchart::ColorDepth::Ansi256;
    if (std::strcmp(v, "16") == 0) return termchart::ColorDepth::Ansi16;
    if (std::strcmp(v, "mono") == 0) return termchart::ColorDepth::None;
    return termchart::ColorDepth::TrueColor;
}

}  // namespace

int main(int argc, char** argv) {
    termchart::raster::RasterKind kind = termchart::raster::RasterKind::Braille;
    termchart::ColorDepth depth = termchart::ColorDepth::TrueColor;  // 强制真彩，重定向也不降级
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--raster") == 0 && i + 1 < argc)
            kind = parse_kind(argv[++i]);
        else if (std::strcmp(argv[i], "--depth") == 0 && i + 1 < argc)
            depth = parse_depth(argv[++i]);
    }

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
        (size.valid() && size.rows > 4u)
            ? static_cast<std::uint32_t>(size.rows) - 1u
            : 24u;

    const std::uint32_t title_rows = 1u;
    const std::uint32_t body = rows - title_rows;

    std::uint32_t sub_x = 2, sub_y = 4;
    switch (kind) {
        case termchart::raster::RasterKind::HalfBlock:
            sub_x = 1; sub_y = 2; break;
        case termchart::raster::RasterKind::Quadrant:
            sub_x = 2; sub_y = 2; break;
        case termchart::raster::RasterKind::Ascii:
        case termchart::raster::RasterKind::Block:
            sub_x = 1; sub_y = 1; break;
        default: break;  // Braille
    }

    termchart::canvas::Canvas canvas(cols, rows);
    const char* depth_name = depth == termchart::ColorDepth::TrueColor ? "truecolor"
        : depth == termchart::ColorDepth::Ansi256   ? "256"
        : depth == termchart::ColorDepth::Ansi16    ? "16"
                                                  : "mono";
    const std::string title = std::string("termchart :: Mandelbrot  [") +
                              termchart::raster::raster_name(kind) +
                              " / " + depth_name + "]";
    draw_title(canvas, title.c_str());

    // 复平面取景：经典主体区，宽高比按字符格 1:2 校正避免纵向拉伸。
    const double cx = -0.5, cy = 0.0;
    const double span_x = 3.4;
    const double span_y = span_x * (2.0 * static_cast<double>(body)) /
                         static_cast<double>(cols);

    const std::uint32_t fw = cols * sub_x;
    const std::uint32_t fh = body * sub_y;
    termchart::image::FloatField shade(fw, fh);
    termchart::image::FloatField tfield(fw, fh);
    for (std::uint32_t py = 0; py < fh; ++py) {
        for (std::uint32_t px = 0; px < fw; ++px) {
            const double wx = cx + ((px + 0.5) / fw - 0.5) * span_x;
            const double wy = cy + ((py + 0.5) / fh - 0.5) * span_y;
            double t = -1.0;
            mandelbrot(wx, wy, t);
            // 亮度型物种（ASCII/Block）用平滑 t 当密度；图像型物种（盲文/半块/象限）
            // 用等值带 fract(t*8)：盲文是二值阈值（v>=0.5 才点亮），连续 t 会被一刀
            // 切没（远场全灭、画面变空心）。等值带形成 8 条同心环，把对数映射后仍偏暗
            // 的远场也显出来，且正好凸显 2×4 子像素分辨率。颜色照旧由 tfield 叠加，
            // 形成"密度 + 色相"双层信息。
            const bool image_kind =
                (kind == termchart::raster::RasterKind::Braille ||
                 kind == termchart::raster::RasterKind::HalfBlock ||
                 kind == termchart::raster::RasterKind::Quadrant);
            const float smooth_t = static_cast<float>(t);
            const float sh = (t < 0.0)
                                 ? 0.0f
                                 : (image_kind ? std::fmod(smooth_t * 8.0f, 1.0f)
                                               : smooth_t);
            shade.set(px, py, sh);
            tfield.set(px, py, smooth_t);
        }
    }
    blit_fractal(shade, tfield, kind, sub_x, sub_y, canvas, 0u, title_rows);

    termchart::render::Renderer renderer(depth);
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    if (termchart::api::write_stdout(bytes) != termchart::ErrorCode::Ok) return 1;
    if (termchart::api::write_stdout("\n") != termchart::ErrorCode::Ok) return 1;
    (void)termchart::api::flush_stdout();
    return terminal.leave() == termchart::ErrorCode::Ok ? 0 : 1;
}
