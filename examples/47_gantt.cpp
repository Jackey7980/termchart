// 47_gantt.cpp —— 甘特图：任务时间条 + 行标签（规格示例）
//
// 消费 chart/cartesian/gantt.hpp：GanttChart。六个任务（有并行、有接续、
// 有跨行重叠），时间轴走自动域；时间单位是抽象的"天"。
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/gantt.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    chart::GanttChart chart;
    chart.set_title("gantt: release plan (days)");
    chart.set_data({
        {"design", 0.0, 3.0},
        {"implement", 2.0, 7.0},
        {"review", 6.0, 8.5},
        {"docs", 7.0, 10.0},
        {"qa", 8.0, 12.0},
        {"release", 12.0, 13.0},
    });

    canvas::Canvas canvas(72, 16);
    chart.draw({canvas});

    render::Renderer renderer;
    const Status status = api::write_stdout(renderer.render_full(canvas.to_buffer()) + "\n");
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
