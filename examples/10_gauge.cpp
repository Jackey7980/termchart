// 10_gauge.cpp —— 仪表盘（规格示例）
//
// 三个量程不同的仪表并排：指针角度 = value / max 的线性映射（由图种内部实现）。
// 演示的是"同一图种、不同配置"的并排布局——canvas 横向三等分，各自平移绘制。
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/polar/gauge.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

struct GaugeSpec {
    std::string label;
    double value;
    Color color;
};

}  // namespace

int main() {
    const std::vector<GaugeSpec> gauges{
        {"cpu", 0.72, Color::ansi16(2)},
        {"mem", 0.45, Color::ansi16(6)},
        {"io", 0.91, Color::ansi16(1)},
    };

    canvas::Canvas canvas(56, 12);
    const std::uint32_t panel = 18;  // 每个仪表的水平面板宽

    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(gauges.size()); ++i) {
        chart::GaugeChart gauge;
        gauge.set_title(gauges[i].label);
        gauge.set_value(gauges[i].value);
        gauge.set_color(gauges[i].color);
        // 平移绘制：借助 Chart 的 margin 语义不可行（margin 是全局的），
        // 因此每个仪表画在自己的子画布上，再 blit 到主画布。
        canvas::Canvas sub(panel, canvas.height());
        gauge.draw({sub});
        for (std::uint32_t y = 0; y < sub.height(); ++y) {
            for (std::uint32_t x = 0; x < sub.width(); ++x) {
                const auto& cell = sub.at(x, y);
                if (cell.cp != U' ') {
                    canvas.put(i * panel + x, y, cell.cp, cell.style);
                }
            }
        }
    }

    render::Renderer renderer;
    (void)api::write_stdout(renderer.render_full(canvas.to_buffer()));
    (void)api::write_stdout("\n");
    (void)api::flush_stdout();
    return 0;
}
