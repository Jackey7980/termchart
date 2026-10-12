// 29_256color_mode.cpp —— 256 色降级（规格示例）
//
// 用 viridis 色表采 16 个色阶铺一行色带，分别在 TrueColor / Ansi256 / Ansi16
// 三档渲染。肉眼可见的结论：
//   * TrueColor 与 Ansi256 在这段色表上几乎一致（256 桶足够密）；
//   * Ansi16 丢层次但保序（暗→亮仍可辨）——这就是降级的价值：信息保序，精度让步。
// 色带本体是"一格一色"的 CellBuffer，走 render/sgr 唯一生成点。
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/color/colormap.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

std::string render_band(ColorDepth depth) {
    const std::span<const Rgb> stops = color::stops(color::ColormapKind::Viridis, 16);
    canvas::Canvas canvas(16, 1);
    for (std::size_t i = 0; i < stops.size(); ++i) {
        canvas.put(static_cast<std::uint32_t>(i), 0, U'█',
                   Style{}.set_fg(Color::rgb(stops[i])));
    }
    render::Renderer renderer(depth);
    std::string out = renderer.render_full(canvas.to_buffer());
    out += "\n";
    return out;
}

}  // namespace

int main() {
    (void)api::write_stdout("== TrueColor ==\n");
    (void)api::write_stdout(render_band(ColorDepth::TrueColor));
    (void)api::write_stdout("== Ansi256 ==\n");
    (void)api::write_stdout(render_band(ColorDepth::Ansi256));
    (void)api::write_stdout("== Ansi16 ==\n");
    (void)api::write_stdout(render_band(ColorDepth::Ansi16));
    (void)api::flush_stdout();
    return 0;
}
