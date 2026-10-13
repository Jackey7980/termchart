// backend/snapshot/png_screenshot.cpp —— CellBuffer → PNG 实现
#include "termchart/backend/snapshot/png_screenshot.hpp"

#include <algorithm>

#include "termchart/color/resolve.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/style.hpp"
#include "termchart/image/bitmap.hpp"
#include "termchart/image/png_encode.hpp"

namespace termchart::backend::snapshot {

void PngScreenshotBackend::set_cell_size(std::uint32_t w, std::uint32_t h) noexcept {
    cell_w_ = std::max<std::uint32_t>(1, w);
    cell_h_ = std::max<std::uint32_t>(1, h);
}

void PngScreenshotBackend::render(const CellBuffer& buffer) {
    out_.clear();
    const std::uint32_t w = buffer.width();
    const std::uint32_t h = buffer.height();
    if (w == 0 || h == 0) {
        return;  // 空 buffer → 空字节串，与 encode_png 对空位图的约定一致
    }

    image::Bitmap img(w * cell_w_, h * cell_h_, 4);
    const color::ResolveOptions resolve{};
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const Cell& cell = buffer.at(x, y);
            const Rgb bg = color::resolve_background(cell.style.bg(), resolve);
            img.fill_rect(x * cell_w_, y * cell_h_, cell_w_, cell_h_, bg.r, bg.g, bg.b, 255);
            if (!is_continuation(cell) && cell.cp != U' ') {
                const Rgb fg = color::resolve_color(cell.style.fg(), resolve);
                img.fill_rect(x * cell_w_, y * cell_h_, cell_w_, cell_h_, fg.r, fg.g, fg.b, 255);
            }
        }
    }
    out_ = image::encode_png(img);
}

std::string render_png_screenshot(const CellBuffer& buffer, std::uint32_t cell_w,
                                  std::uint32_t cell_h) {
    PngScreenshotBackend backend;
    backend.set_cell_size(cell_w, cell_h);
    return render_snapshot(backend, buffer);
}

}  // namespace termchart::backend::snapshot
