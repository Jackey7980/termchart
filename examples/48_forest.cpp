// 48_forest.cpp —— 森林图：各研究点估计 + CI 横线 + 合计菱形（规格示例）
//
// 消费 chart/statistical/forest.hpp：ForestChart。五个研究 + 合计行；
// CI 直接按给定的 [lower, upper] 画，x = 0 参考线由库自动并入域。
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/statistical/forest.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    chart::ForestChart chart;
    chart.set_title("forest plot: effect size per study");

    std::vector<chart::ForestStudy> studies;
    studies.push_back({"study a", 0.52, 0.18, 0.86});
    studies.push_back({"study b", 0.31, -0.05, 0.67});
    studies.push_back({"study c", 0.66, 0.40, 0.92});
    studies.push_back({"study d", 0.12, -0.22, 0.46});
    studies.push_back({"study e", 0.44, 0.15, 0.73});
    chart.set_studies(std::move(studies));
    chart.set_total({"pooled", 0.41, 0.27, 0.55});

    canvas::Canvas canvas(72, 18);
    chart.draw({canvas});

    render::Renderer renderer;
    const Status status = api::write_stdout(renderer.render_full(canvas.to_buffer()) + "\n");
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
