// 44_violin.cpp —— 小提琴图：核密度轮廓（规格示例）
//
// 消费 chart/cartesian/violin.hpp：ViolinChart。三组确定性样本（不同中心的
// 正弦合成），密度带数自动（随绘图区高度），中位数线默认开启。
#include <cmath>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/violin.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

std::vector<double> group(int n, double center, double amp, double freq, double phase) {
    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        values.push_back(center + amp * std::sin(freq * i + phase));
    }
    return values;
}

}  // namespace

int main() {
    chart::ViolinChart chart;
    chart.set_title("violin: three deterministic groups");
    chart.set_data({group(48, 0.0, 0.8, 0.35, 0.0),
                    group(48, 1.5, 0.5, 0.22, 1.0),
                    group(48, -1.2, 1.1, 0.50, 2.0)});

    canvas::Canvas canvas(72, 20);
    chart.draw({canvas});

    render::Renderer renderer;
    const Status status = api::write_stdout(renderer.render_full(canvas.to_buffer()) + "\n");
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
