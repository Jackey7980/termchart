// 37_export_latex.cpp —— 导出 LaTeX TikZ（规格示例）
//
// 同一份 CellBuffer 两次导出，对照 LatexOptions 的两个开关：
//   * 默认：片段模式（tikzpicture，可直接嵌进 LaTeX 文档）；
//   * standalone：独立可编译文档（documentclass + tikz 库声明）。
// 导出走 backend/vector/latex_backend（与 svg_backend 同款生命周期），
// 终端渲染链路不动——导出只发生在输出边界。
#include <cstdint>
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/backend/vector/latex_backend.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/style.hpp"

namespace {

using namespace termchart;

// 一张确定性小柱状图：█ 实体柱 + ─ 基线 + 标签（非空格格都会进 TikZ 节点）。
canvas::Canvas make_chart() {
    canvas::Canvas canvas(24, 8);
    const Style bar = Style{}.set_fg(Color::ansi16(6));
    const int heights[5] = {3, 5, 2, 6, 4};
    for (int i = 0; i < 5; ++i) {
        for (int h = 0; h < heights[i]; ++h) {
            canvas.put(static_cast<std::uint32_t>(2 + i * 4),
                       static_cast<std::uint32_t>(6 - h), U'█', bar);
        }
        canvas.draw_text(Point{static_cast<double>(1 + i * 4), 7.0},
                         std::to_string(i + 1), Style{});
    }
    canvas.draw_text(Point{1.0, 0.0}, "weekly output", Style{}.set_attr(Attr::Bold));
    return canvas;
}

void emit(const std::string& caption, const std::string& tikz) {
    (void)api::write_stdout("== " + caption + " ==\n");
    (void)api::write_stdout(tikz);
    (void)api::write_stdout("\n");
}

}  // namespace

int main() {
    const canvas::Canvas chart = make_chart();
    const termchart::CellBuffer buffer = chart.to_buffer();

    // 默认 standalone=true：先出片段（裸 tikzpicture），再出独立文档。
    backend::vector::LatexOptions fragment_opts;
    fragment_opts.standalone = false;
    backend::vector::LatexBackend fragment;
    fragment.set_options(fragment_opts);
    (void)fragment.begin();
    fragment.render(buffer);
    (void)fragment.end();
    emit("片段模式（tikzpicture）", fragment.str());

    backend::vector::LatexBackend standalone;
    standalone.set_options(backend::vector::LatexOptions{});  // 默认即 standalone
    (void)standalone.begin();
    standalone.render(buffer);
    (void)standalone.end();
    emit("standalone 模式（独立可编译）", standalone.str());

    (void)api::flush_stdout();
    return 0;
}
