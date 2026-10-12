// 09_radar.cpp —— 雷达图（规格示例）
//
// 多维能力对比：雷达网（参考多边形）+ 数据多边形 + 轴标签。
// 数据是确定性的（不引随机数）；本图种没有手势，与脚本/交互模式无关。
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/polar/radar.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/term/size.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    chart::RadarChart chart;
    chart.set_title("engine capability");
    chart.set_series_name("termchart");
    chart.set_color(Color::ansi16(6));
    const std::vector<std::string> axes{"render", "color", "dsl", "graph", "chart", "i18n"};
    std::vector<double> values;
    for (std::size_t i = 0; i < axes.size(); ++i) {
        // 0.55 ~ 0.95 的确定性"能力值"。
        values.push_back(0.75 + 0.2 * std::sin(1.7 * static_cast<double>(i)));
    }
    chart.set_values(values, axes);

    const term::TerminalSize size = term::query_size();
    const int rows = size.rows == 0 ? 24 : size.rows;
    canvas::Canvas canvas(56, static_cast<std::uint32_t>(rows > 8 ? rows - 4 : 14));
    chart.draw({canvas});

    render::Renderer renderer;
    (void)api::write_stdout(renderer.render_full(canvas.to_buffer()));
    (void)api::write_stdout("\n");
    (void)api::flush_stdout();
    return 0;
}
