// 27_background_thread.cpp —— 后台线程投递：生产线程 post，主线程 join 后消费
//
// 消费 app/post.hpp：PostQueue（闭包队列）+ Mailbox<T>（最新值信箱）。
// 时序：投递 → join → drain（闭包在主线程执行，顺序 = 投递顺序）→ 用消费到的
// 数据画图。没有忙等、没有条件变量——join 就是同步点，CI 里时序完全确定。
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <thread>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/app/post.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    app::PostQueue queue;
    app::Mailbox<double> mailbox;
    std::vector<double> received;
    received.reserve(40);

    // 生产线程只投递：不渲染、不碰终端（post.hpp 的使用边界）。
    std::thread producer([&queue, &mailbox, &received] {
        for (int i = 0; i < 40; ++i) {
            const double v = std::sin(static_cast<double>(i) / 3.0);
            queue.post([&received, v] { received.push_back(v); });
            mailbox.post(v);  // 最新值语义：未消费的旧值会被覆盖
        }
    });
    producer.join();

    // join 之后消费：闭包全在主线程执行，无数据竞争。
    const std::size_t executed = queue.drain();
    double latest = 0.0;
    const bool has_latest = mailbox.consume(latest);

    chart::LineChart chart;
    chart.set_title("streamed samples (produced on a background thread)");
    chart::Series series;
    series.name = "sample";
    series.color = Color::ansi16(2);
    for (std::size_t i = 0; i < received.size(); ++i) {
        series.points.push_back(Point{static_cast<double>(i), received[i]});
    }
    chart.add_series(std::move(series));

    canvas::Canvas canvas(72, 18);
    chart.draw({canvas});
    char readout[96] = {};
    std::snprintf(readout, sizeof(readout),
                  "tasks executed=%zu  mailbox latest=%.3f (has=%d)", executed, latest,
                  has_latest ? 1 : 0);
    canvas.draw_text(Point{0.0, static_cast<double>(canvas.height() - 1)}, readout,
                     Style{}.set_fg(Color::ansi16(8)));

    render::Renderer renderer;
    const Status status = api::write_stdout(renderer.render_full(canvas.to_buffer()));
    api::flush_stdout();
    return (status == ErrorCode::Ok && executed == 40 && has_latest) ? 0 : 1;
}
