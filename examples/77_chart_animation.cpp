// 77_chart_animation.cpp —— 图表层动画：按名进度 + 折线扫入 / 柱从基线生长
//
// 三个要点：
//   1. 动画**只发生在 chart 层**：Canvas 与 Scene 完全不知道有动画这回事，
//      没有新增字段、没有全局状态、没有破坏性修改 IR；
//   2. 时间由调用方 tick() 注入（首次 tick 即起点），全程不读真实时钟——
//      同一份输出在任何机器上逐字节一致，headless / CI 可直接快照；
//   3. 轴域始终按**完整数据**算：动画只影响"画到哪"，不影响"坐标轴多长"，
//      否则画面会一边生长一边缩放，看起来像镜头在动。
#include <cstdio>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/bar_chart.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

constexpr double kDuration = 1.0;  // 秒；chart 层的时长单位与 util/timer 一致

// 取 4 帧：起点、四分之一、二分之一、终点。
const std::vector<double> kFrames{0.0, 0.25, 0.5, 1.0};

bool emit(render::Renderer& renderer, const canvas::Canvas& canvas, const char* what, double t) {
    char head[128] = {};
    std::snprintf(head, sizeof(head), "\n-- %s   t=%.2fs --\n", what, t);
    bool ok = api::write_stdout(head) == ErrorCode::Ok;
    ok = (api::write_stdout(renderer.render_full(canvas.to_buffer())) == ErrorCode::Ok) && ok;
    return ok && api::write_stdout("\n") == ErrorCode::Ok;
}

}  // namespace

int main() {
    render::Renderer renderer;
    bool ok = true;

    // ── 折线：从左向右扫出 ────────────────────────────────────────────
    {
        chart::LineChart chart;
        chart.set_title("月度收入");
        chart::Series s;
        s.name = "利润线";
        s.color = Color::ansi16(2);
        s.points = {{0.0, 2.0}, {1.0, 5.0}, {2.0, 3.0}, {3.0, 8.0},
                    {4.0, 6.0}, {5.0, 11.0}, {6.0, 9.0}};
        chart.add_series(std::move(s));
        // 一行声明动画：名字就是图例上那行文本。
        chart.add_animation("利润线", kDuration, chart::Easing::EaseOutCubic);

        for (const double t : kFrames) {
            chart.tick(t);
            canvas::Canvas canvas(56, 16);
            chart.draw(chart::DrawContext{canvas});
            ok = emit(renderer, canvas, "LineChart", t) && ok;
        }
    }

    // ── 柱状：从基线 0 长到目标值（负值向下长）────────────────────────
    {
        chart::BarChart chart;
        chart.set_title("季度环比");
        chart.set_values({5.0, -3.0, 8.0, 2.0}, {"Q1", "Q2", "Q3", "Q4"}, Color::ansi16(6),
                         "环比");
        chart.add_animation("环比", kDuration, chart::Easing::EaseOutQuad);

        for (const double t : kFrames) {
            chart.tick(t);
            canvas::Canvas canvas(56, 16);
            chart.draw(chart::DrawContext{canvas});
            ok = emit(renderer, canvas, "BarChart", t) && ok;
        }
        // 动画跑完后 animating() 变 false，渲染循环据此停下。
        std::printf("\nanimating() after 1.00s = %s\n", chart.animating() ? "true" : "false");
    }

    api::flush_stdout();
    return ok ? 0 : 1;
}
