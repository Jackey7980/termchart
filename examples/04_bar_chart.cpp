// 04_bar_chart.cpp —— 柱状图（阶段出口条件）
//
// 与 03 同源：BarChart 把单系列值画成竖条，自动按 nice 域留白对齐。
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/bar_chart.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    canvas::Canvas canvas(80, 24);

    chart::BarChart chart;
    chart.set_title("monthly");

    std::vector<double> values{4.0, 9.0, 6.0, 12.0, 8.0, 15.0, 10.0};
    std::vector<std::string> cats{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul"};
    chart.set_values(values, cats, Color::ansi16(4), "sales");
    chart.draw({canvas});

    render::Renderer renderer;
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    const Status st = api::write_stdout(bytes);
    api::flush_stdout();
    return st == ErrorCode::Ok ? 0 : 1;
}
