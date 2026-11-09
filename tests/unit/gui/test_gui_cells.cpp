// test_gui_cells.cpp —— 字符网格 → 位图（gui::render_cells_to_bitmap）
//
// 关注契约：
//   * 盲文按 Unicode 的 2×4 点阵**画点**（不是当字形渲染）：U+2801 只点亮左上一点；
//   * 半块按字符语义：▀ 上半=前景色、下半=背景色（热力图靠它表达两个数值）；
//   * 宽字符的续接格不产出像素（补空格会盖住字形右半边）；
//   * 未指定背景的格子保持画布底色（不糊成默认背景）。
#include <doctest.h>

#include <cstdint>

#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/style.hpp"
#include "termchart/gui/cells.hpp"

namespace {

using namespace termchart;
using namespace termchart::gui;

// 读一个像素的 RGB（忽略 alpha）。
Rgb pixel_at(const image::Bitmap& bmp, std::uint32_t x, std::uint32_t y) {
    return Rgb{bmp.at(x, y, 0), bmp.at(x, y, 1), bmp.at(x, y, 2)};
}

}  // namespace

TEST_CASE("gui/cells: 盲文按 2×4 点阵画点，不是当字形渲染") {
    CellBuffer cells(1, 1);
    cells.at(0, 0) = Cell{U'\u2801', Style{}.set_fg(Color::rgb(255, 0, 0))};

    const CellBitmapOptions opts{.cell_w = 8.0, .cell_h = 16.0};
    const image::Bitmap bmp = render_cells_to_bitmap(cells, opts);
    REQUIRE(bmp.width() == 8);
    REQUIRE(bmp.height() == 16);

    // U+2801 = 只亮左上点（子像素行 0、列 0）⇒ 该点为前景红，其余为底色。
    const Rgb red{255, 0, 0};
    CHECK(pixel_at(bmp, 1, 1) == red);
    // 右列（子像素列 1）不该有点：右上角仍是底色。
    CHECK(pixel_at(bmp, 6, 1) == opts.background);
    // 下半部分整体是底色（U+2801 没点下面三行）。
    CHECK(pixel_at(bmp, 1, 12) == opts.background);
}

TEST_CASE("gui/cells: 半块上=前景色、下=背景色") {
    CellBuffer cells(1, 1);
    cells.at(0, 0) = Cell{U'\u2580', Style{}.set_fg(Color::rgb(0, 255, 0))
                                        .set_bg(Color::rgb(0, 0, 255))};

    const CellBitmapOptions opts{.cell_w = 8.0, .cell_h = 16.0};
    const image::Bitmap bmp = render_cells_to_bitmap(cells, opts);

    // 上半 = 绿，下半 = 蓝。
    CHECK(pixel_at(bmp, 4, 2) == Rgb{0, 255, 0});
    CHECK(pixel_at(bmp, 4, 13) == Rgb{0, 0, 255});
}

TEST_CASE("gui/cells: 宽字符续接格不产出像素") {
    CellBuffer cells(2, 1);
    cells.at(0, 0) = Cell{U'A', Style{}};
    cells.at(1, 0) = Cell{kContinuationCp, Style{}};

    const CellBitmapOptions opts{.cell_w = 8.0, .cell_h = 16.0};
    const image::Bitmap bmp = render_cells_to_bitmap(cells, opts);
    REQUIRE(bmp.width() == 16);
    // 第二格只有底色（续接格不画字形，也不画背景块）。
    CHECK(pixel_at(bmp, 12, 8) == opts.background);
}

TEST_CASE("gui/cells: 未指定背景的格子保持画布底色") {
    CellBuffer cells(1, 1);
    cells.at(0, 0) = Cell{U' ', Style{}};  // 无背景色

    const CellBitmapOptions opts{.cell_w = 4.0, .cell_h = 8.0, .background = Rgb{1, 2, 3}};
    const image::Bitmap bmp = render_cells_to_bitmap(cells, opts);
    CHECK(pixel_at(bmp, 1, 4) == Rgb{1, 2, 3});
}
