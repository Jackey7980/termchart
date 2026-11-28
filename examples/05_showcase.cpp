// 05_showcase.cpp —— 子像素光栅 + 真彩旗舰展示
//
// 同一个 3D 带光球体，用项目最强的两层"取字符"能力各画一遍：
//   上屏：Braille（2×4 子像素，每字符 8 点阵）—— 分辨率最高，逐格 Turbo 彩虹真彩。
//   下屏：HalfBlock（1×2 子像素，每字符上下两色）—— 每字符纵向承载 2 个独立着色的子像素。
//
// 展示完整链路：离散几何（image::FloatField）→ 取字符（raster）→ 真彩（color::colormap）
// → 终端（render）。两个面板共享同一套采样函数，只是光栅器与取色策略不同。
//
// 跑法（Windows 终端，cmd）： chcp 65001 && build\ninja\examples\05_showcase.exe
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
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
#include <cstdlib>
#include <fstream>
#include "termchart/gui/cells.hpp"
#include "termchart/media/image/png_encode.hpp"

namespace {

constexpr char32_t kBrailleBlank = U'\u2800';  // 盲文熄灭格
constexpr char32_t kSpace = U'\u0020';          // 其余物种的熄灭格
constexpr char32_t kUpperHalf = U'\u2580';      // ▀ 上半块：上亮→前景
constexpr char32_t kLowerHalf = U'\u2584';      // ▄ 下半块：下亮→前景
constexpr char32_t kFullBlock = U'\u2588';      // █ 全块

// 在 (fw×fh) 子像素场上对 (px,py) 采样一个 3D 球体：
// 命中则返回 true，并给出 Lambert 明暗 shade∈[0,1] 与彩虹色相 hue∈[0,1]。
// 子像素在屏幕上各向同性（盲文/半块的每 subpixel 都是 ≈0.5 屏宽×0.5 屏高），
// 故 nx/ny 直接均匀映射到 [-1,1]，球面自然是正圆，无需长宽比拉伸。
bool sample_sphere(double px, double py, double fw, double fh, double& shade,
                   double& hue) {
    const double nx = (px + 0.5) / fw * 2.0 - 1.0;
    const double ny = (py + 0.5) / fh * 2.0 - 1.0;
    const double r2 = nx * nx + ny * ny;
    if (r2 > 1.0) {
        shade = 0.0;
        hue = 0.0;
        return false;
    }
    const double nz = std::sqrt(1.0 - r2);
    // 光照方向（左上前方）
    constexpr double lx = -0.40, ly = 0.55, lz = 0.73;
    // const double（非 constexpr）：libc++ 的 std::sqrt 不是 constexpr 函数，
    // macOS/Apple Clang 编译不过。
    const double ll = std::sqrt(lx * lx + ly * ly + lz * lz);
    const double lambert = (nx * lx + ny * ly + nz * lz) / ll;
    shade = 0.16 + 0.84 * std::max(0.0, lambert);
    // 色相绕球体一圈彩虹
    hue = 0.5 + 0.5 * (std::atan2(ny, nx) / std::acos(-1.0));
    return true;
}

termchart::Color colorize(double hue, double shade) {
    const termchart::Rgb base = termchart::color::sample(
        termchart::color::ColormapKind::Turbo, std::clamp(hue, 0.0, 1.0));
    const double b = 0.30 + 0.70 * shade;  // 明暗调制亮度
    return termchart::Color::rgb(
        static_cast<std::uint8_t>(std::lround(base.r * b)),
        static_cast<std::uint8_t>(std::lround(base.g * b)),
        static_cast<std::uint8_t>(std::lround(base.b * b)));
}

// Braille 版：逐字符单色（整格 8 点阵共用一格颜色）。用格中心子像素取色。
void blit_braille(const termchart::image::FloatField& shade_field,
                  const std::vector<float>& hue_field,
                  termchart::canvas::Canvas& canvas, std::uint32_t ox,
                  std::uint32_t oy) {
    termchart::raster::RasterOptions opts;
    opts.fg = termchart::Color::ansi16(7);
    opts.bg = termchart::Color::ansi16(0);
    termchart::CellBuffer buf = termchart::raster::rasterize(
        shade_field, termchart::raster::RasterKind::Braille, opts);
    const std::uint32_t fw = shade_field.width();
    for (std::uint32_t y = 0; y < buf.height(); ++y) {
        for (std::uint32_t x = 0; x < buf.width(); ++x) {
            const termchart::Cell& c = buf.at(x, y);
            if (c.cp == kBrailleBlank) continue;
            const std::uint32_t spx = x * 2u + 1u;
            const std::uint32_t spy = y * 4u + 2u;
            const double hue = hue_field[static_cast<std::size_t>(spy) * fw + spx];
            const double shade = shade_field.at(spx, spy);
            termchart::Style s;
            s.set_fg(colorize(hue, shade));
            canvas.put(ox + x, oy + y, c.cp, s);
        }
    }
}

// HalfBlock 版：每字符上下两个子像素各取一色（fg=上 / bg=下，按字形修正）。
void blit_halfblock(const termchart::image::FloatField& shade_field,
                    const std::vector<float>& hue_field,
                    termchart::canvas::Canvas& canvas, std::uint32_t ox,
                    std::uint32_t oy) {
    termchart::raster::RasterOptions opts;
    opts.fg = termchart::Color::ansi16(7);
    opts.bg = termchart::Color::ansi16(0);
    termchart::CellBuffer buf = termchart::raster::rasterize(
        shade_field, termchart::raster::RasterKind::HalfBlock, opts);
    const std::uint32_t fw = shade_field.width();
    for (std::uint32_t y = 0; y < buf.height(); ++y) {
        for (std::uint32_t x = 0; x < buf.width(); ++x) {
            const termchart::Cell& c = buf.at(x, y);
            if (c.cp == kSpace) continue;
            const std::uint32_t top = static_cast<std::size_t>(y) * 2u;
            const std::uint32_t bot = top + 1u;
            const double hueT = hue_field[static_cast<std::size_t>(top) * fw + x];
            const double hueB = hue_field[static_cast<std::size_t>(bot) * fw + x];
            const double shT = shade_field.at(x, top);
            const double shB = shade_field.at(x, bot);
            const termchart::Color cT = colorize(hueT, shT);
            const termchart::Color cB = colorize(hueB, shB);
            termchart::Style s;
            if (c.cp == kUpperHalf) {        // ▀：上亮→fg，下→bg
                s.set_fg(cT).set_bg(cB);
            } else if (c.cp == kLowerHalf) {  // ▄：下亮→fg，上→bg
                s.set_fg(cB).set_bg(cT);
            } else {                          // █：整块 fg
                s.set_fg(cT);
            }
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
        (size.valid() && size.rows > 8u)
            ? static_cast<std::uint32_t>(size.rows) - 1u
            : 24u;

    const std::uint32_t title_rows = 1u;
    const std::uint32_t gap = 1u;
    const std::uint32_t body = rows - title_rows - gap;
    const std::uint32_t braille_rows = body * 3u / 5u;
    const std::uint32_t half_rows = body - braille_rows;

    termchart::canvas::Canvas canvas(cols, rows);
    draw_title(canvas,
               "termchart showcase :: braille (top) + halfblock (bottom) - Turbo colormap");

    // ── 上屏：Braille（2×4 子像素，逐格真彩）──
    {
        const std::uint32_t fw = cols * 2u;
        const std::uint32_t fh = braille_rows * 4u;
        termchart::image::FloatField shade(fw, fh);
        std::vector<float> hue(static_cast<std::size_t>(fw) * fh, 0.0f);
        for (std::uint32_t py = 0; py < fh; ++py) {
            for (std::uint32_t px = 0; px < fw; ++px) {
                double sh = 0.0, h = 0.0;
                if (sample_sphere(static_cast<double>(px), static_cast<double>(py),
                                  static_cast<double>(fw), static_cast<double>(fh),
                                  sh, h)) {
                    shade.set(px, py, static_cast<float>(sh));
                    hue[static_cast<std::size_t>(py) * fw + px] = static_cast<float>(h);
                }
            }
        }
        blit_braille(shade, hue, canvas, 0u, title_rows);
    }

    // ── 下屏：HalfBlock（1×2 双色真彩）──
    {
        const std::uint32_t fw = cols;
        const std::uint32_t fh = half_rows * 2u;
        termchart::image::FloatField shade(fw, fh);
        std::vector<float> hue(static_cast<std::size_t>(fw) * fh, 0.0f);
        for (std::uint32_t py = 0; py < fh; ++py) {
            for (std::uint32_t px = 0; px < fw; ++px) {
                double sh = 0.0, h = 0.0;
                if (sample_sphere(static_cast<double>(px), static_cast<double>(py),
                                  static_cast<double>(fw), static_cast<double>(fh),
                                  sh, h)) {
                    shade.set(px, py, static_cast<float>(sh));
                    hue[static_cast<std::size_t>(py) * fw + px] = static_cast<float>(h);
                }
            }
        }
        blit_halfblock(shade, hue, canvas, 0u,
                       title_rows + braille_rows + gap);
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
