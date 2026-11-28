// cells.hpp —— 字符网格（CellBuffer）→ 位图：把"终端里看到的画面"直接存成 PNG
//
// 为什么补这条通路：库里原本只有两条位图出口——
//   * image::encode_png(Bitmap)：位图编码；
//   * gui::SceneRenderer：Scene IR → 位图（矢量语义 + 5×7 点阵字形）。
// 但**终端画面本身**（canvas::Canvas::to_buffer() 的字符网格 + 每格样式）没有位图出口：
// 想把终端里真实看到的样子存成 PNG，只能靠外部工具去解析 ANSI 字节流——那是旁路，
// 引擎不参与，画质与终端实际渲染脱节（颜色被降级、盲文/半块被当普通字形处理）。
//
// 本文件补上正路：CellBuffer → Bitmap，字形语义与终端一致——
//   * 盲文 U+2800–U+28FF：按 Unicode Braille Patterns 的 2×4 点阵**直接画点**
//     （盲文在终端里本就是点阵语义，不是字形）；
//   * 方块元素 U+2580–U+259F：按字符语义画矩形/与背景混色（半块热力图靠它表达数值，
//     上半=前景色、下半=背景色）；
//   * 其余码点：gui::draw_text（内置 5×7 点阵 + 系统 TTF，CJK 走 TTF）。
//
// 用途：README 配图、文档插图、把终端里好看的样子存成图片分享。
#ifndef TERMCHART_GUI_CELLS_HPP
#define TERMCHART_GUI_CELLS_HPP

#include <cstdint>

#include "termchart/core/rgb.hpp"
#include "termchart/export.hpp"
#include "termchart/media/image/bitmap.hpp"

namespace termchart {

// 字符网格在 core（core/buffer.hpp）——前向声明只为了按 const 引用接收。
class CellBuffer;

}  // namespace termchart

namespace termchart::gui {

struct CellBitmapOptions {
    double cell_w = 9.0;    // 每格宽（像素）
    double cell_h = 16.0;   // 每格高（像素）
    Rgb background{13, 17, 23};    // 画布底色（Style 未指定背景时也用它）
    Rgb foreground{201, 209, 217}; // Style 未指定前景时用它
    bool transparent = false;       // 背景 alpha=0（供再合成）
};

// 字符网格 → 位图。宽字符的续接格（kContinuationCp）不产出任何像素——
// 终端绘制宽字形时自动占两列，补空格反而会盖住右半边。
[[nodiscard]] TERMCHART_API image::Bitmap render_cells_to_bitmap(
    const CellBuffer& cells, const CellBitmapOptions& options = {});

}  // namespace termchart::gui

#endif  // TERMCHART_GUI_CELLS_HPP
