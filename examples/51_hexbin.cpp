// 51_hexbin.cpp —— 六边形分箱：偏移网格近似铺排 + 计数分级着色（规格示例）
//
// 消费 chart/geo/hexbin.hpp：Hexbin。两个确定性点簇（正弦扰动的中心聚集，
// 无随机数），桶半径 2、计数分 5 档；精度立场见头文件——不做假精度。
#include <cmath>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/geo/hexbin.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

void add_cluster(chart::Hexbin& bin, double cx, double cy, int n, double rx, double ry) {
    for (int k = 0; k < n; ++k) {
        const double x = cx + rx * std::sin(1.3 * k);
        const double y = cy + ry * std::cos(0.9 * k);
        bin.add_point(x, y);
    }
}

}  // namespace

int main() {
    chart::Hexbin chart;
    chart.set_title("hexbin: two clusters, count-graded shading");
    add_cluster(chart, 12.0, 6.0, 40, 3.0, 2.0);
    add_cluster(chart, 30.0, 12.0, 40, 4.0, 2.5);

    chart::HexbinOptions opts;
    opts.radius = 2.0;
    opts.bins = 5;
    chart.set_options(opts);

    canvas::Canvas canvas(72, 20);
    chart.draw({canvas});

    render::Renderer renderer;
    const Status status = api::write_stdout(renderer.render_full(canvas.to_buffer()) + "\n");
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
