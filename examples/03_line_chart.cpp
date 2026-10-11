// 03_line_chart.cpp —— 带坐标轴的折线图（阶段出口条件）
//
// 演示 stage 3 垂直切片：Series → LineChart → canvas::Canvas → render → stdout。
// 注意：本示例不直接碰 render 层之外任何东西，图表内部用 layout/scale 自洽完成。
#include <cmath>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/core/style.hpp"
#include "termchart/core/point.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    canvas::Canvas canvas(80, 24);

    chart::LineChart chart;
    chart.set_title("sine wave");

    chart::Series s;
    s.name = "sin(x)";
    s.color = Color::ansi16(2);
    for (int i = 0; i <= 80; ++i) {
        const double x = static_cast<double>(i) / 8.0;
        s.points.push_back(Point{x, std::sin(x)});
    }
    chart.add_series(std::move(s));
    chart.draw({canvas});

    render::Renderer renderer;
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    const Status st = api::write_stdout(bytes);
    api::flush_stdout();
    return st == ErrorCode::Ok ? 0 : 1;
}
