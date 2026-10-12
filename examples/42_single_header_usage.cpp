// 42_single_header_usage.cpp —— 单头产物自包含验证（阶段 7）
//
// 只 include 生成的单头 single_include/termchart.hpp，不碰 include/ 下的任何头，
// 证明单头自包含可用。本示例需以 -I single_include（且先于 -I include）编译，
// 使 "termchart.hpp" 解析到单头产物而非伞形头；接线见 examples/CMakeLists.txt。
#include <cmath>
#include <vector>

#include "termchart.hpp"

int main() {
    termchart::api::init_core();

    // 最简图表：一段正弦折线，走 Series → LineChart → Canvas → Renderer 全链路。
    termchart::canvas::Canvas canvas(60, 20);
    termchart::chart::LineChart chart;
    chart.set_title("single header");

    termchart::chart::Series s;
    s.name = "sin(x)";
    s.color = termchart::Color::ansi16(2);
    for (int i = 0; i <= 60; ++i) {
        const double x = static_cast<double>(i) / 6.0;
        s.points.push_back(termchart::Point{x, std::sin(x)});
    }
    chart.add_series(std::move(s));
    chart.draw({canvas});

    termchart::render::Renderer renderer;
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    const termchart::Status st = termchart::api::write_stdout(bytes);
    termchart::api::flush_stdout();
    return st == termchart::ErrorCode::Ok ? 0 : 1;
}
