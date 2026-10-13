// 45_ridgeline.cpp —— 山脊图：多组分布按行堆叠、后行遮前行（规格示例）
//
// 消费 chart/cartesian/ridgeline.hpp：RidgelineChart。四组确定性样本，
// 组名/颜色随组给定；band_height 与 bins 走显式配置，图例默认开启。
#include <cmath>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/ridgeline.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

std::vector<double> samples(int n, double amp, double freq, double phase) {
    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        values.push_back(amp * std::sin(freq * i + phase));
    }
    return values;
}

}  // namespace

int main() {
    chart::RidgelineChart chart;
    chart.set_title("ridgeline: overlapping density rows");

    std::vector<chart::RidgelineChart::Group> groups;
    groups.push_back({"north", samples(64, 0.9, 0.31, 0.0), Color::ansi16(6)});
    groups.push_back({"south", samples(64, 0.7, 0.47, 1.3), Color::ansi16(2)});
    groups.push_back({"east", samples(64, 1.1, 0.23, 2.1), Color::ansi16(4)});
    groups.push_back({"west", samples(64, 0.8, 0.39, 3.2), Color::ansi16(3)});
    chart.set_groups(std::move(groups));

    chart::RidgelineChart::Options opts;
    opts.band_height = 4;
    opts.bins = 24;
    opts.show_legend = true;
    chart.set_options(opts);

    canvas::Canvas canvas(72, 20);
    chart.draw({canvas});

    render::Renderer renderer;
    const Status status = api::write_stdout(renderer.render_full(canvas.to_buffer()) + "\n");
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
