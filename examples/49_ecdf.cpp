// 49_ecdf.cpp —— 经验分布函数：逐级跳台阶（规格示例）
//
// 消费 chart/statistical/ecdf.hpp：EcdfChart。40 个确定性样本（周期取模
// 生成，值域 [-2.5, 2.0)，含重复值——重复值应合并为一次完整抬升）；
// 台阶竖线与样本圆点默认开启，y 域固定 [0, 1]。
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/statistical/ecdf.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    chart::EcdfChart chart;
    chart.set_title("ecdf: 40 deterministic samples");

    std::vector<double> values;
    values.reserve(40);
    for (int i = 0; i < 40; ++i) {
        values.push_back(static_cast<double>((i * 7) % 45) / 10.0 - 2.5);
    }
    chart.set_values(std::move(values), Color::ansi16(4), "sample");

    canvas::Canvas canvas(72, 18);
    chart.draw({canvas});

    render::Renderer renderer;
    const Status status = api::write_stdout(renderer.render_full(canvas.to_buffer()) + "\n");
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
