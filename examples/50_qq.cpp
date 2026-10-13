// 50_qq.cpp —— QQ 图：样本分位数对正态理论分位数（规格示例）
//
// 消费 chart/cartesian/qq.hpp：QQChart。样本 = 三个不同频率正弦的叠加
// （中心极限意义下近似对称分布），μ/σ 由库按样本估计并画 y = μ + σx 参考线；
// 点贴线即正态。
#include <cmath>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/qq.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    chart::QQChart chart;
    chart.set_title("qq plot: sample vs normal quantiles");

    std::vector<double> sample;
    sample.reserve(60);
    for (int i = 0; i < 60; ++i) {
        sample.push_back(std::sin(0.7 * i) + std::sin(1.9 * i) + std::sin(3.1 * i));
    }
    chart.set_sample(std::move(sample));

    canvas::Canvas canvas(72, 18);
    chart.draw({canvas});

    render::Renderer renderer;
    const Status status = api::write_stdout(renderer.render_full(canvas.to_buffer()) + "\n");
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
