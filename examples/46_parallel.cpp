// 46_parallel.cpp —— 平行坐标：每维一条竖轴，观测跨轴连折线（规格示例）
//
// 消费 chart/cartesian/parallel.hpp：ParallelChart。四个维度、六条观测，
// 数值确定性生成；轴与维度名默认开启，折线颜色按观测轮换。
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/parallel.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    chart::ParallelChart chart;
    chart.set_title("parallel coordinates: six hosts x four metrics");

    // 观测值确定性生成：三组"形态"——正相关、反相关、平稳。
    std::vector<std::vector<double>> observations;
    for (int i = 0; i < 2; ++i) {
        observations.push_back({10.0 + i, 40.0 + 3.0 * i, 5.0 + 0.4 * i, 1.1 - 0.1 * i});
    }
    for (int i = 0; i < 2; ++i) {
        observations.push_back({90.0 - 4.0 * i, 20.0 + 6.0 * i, 9.0 - 0.8 * i, 0.2 + 0.3 * i});
    }
    observations.push_back({50.0, 55.0, 6.0, 0.7});
    observations.push_back({52.0, 53.0, 6.2, 0.7});

    chart.set_axes({"cpu", "mem", "io", "net"}, std::move(observations));

    canvas::Canvas canvas(72, 20);
    chart.draw({canvas});

    render::Renderer renderer;
    const Status status = api::write_stdout(renderer.render_full(canvas.to_buffer()) + "\n");
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
