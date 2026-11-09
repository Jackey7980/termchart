// 43_contour.cpp —— 等高线：标量场 → marching squares（规格示例）
//
// 消费 chart/cartesian/contour.hpp：ContourChart。场按行给定（row 0 = 最上），
// 等值线级数走自动均分（level_count）；确定性标量场 = sin(x/3)·cos(y/3)。
#include <cmath>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/contour.hpp"
#include "termchart/render/renderer.hpp"
#include <cstdlib>
#include <fstream>
#include "termchart/gui/cells.hpp"
#include "termchart/image/png_encode.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    chart::ContourChart chart;
    chart.set_title("contour: sin(x/3) * cos(y/3)");

    chart::ContourChart::Options opts;
    opts.color = Color::ansi16(6);
    opts.level_count = 5;
    chart.set_options(std::move(opts));

    std::vector<std::vector<double>> rows;
    rows.reserve(16);
    for (int j = 0; j < 16; ++j) {
        std::vector<double> row;
        row.reserve(24);
        for (int i = 0; i < 24; ++i) {
            row.push_back(std::sin(i / 3.0) * std::cos(j / 3.0));
        }
        rows.push_back(std::move(row));
    }
    chart.set_field(std::move(rows));

    canvas::Canvas canvas(72, 20);
    chart.draw({canvas});

    render::Renderer renderer;
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
    const Status status = api::write_stdout(renderer.render_full(canvas.to_buffer()) + "\n");
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
