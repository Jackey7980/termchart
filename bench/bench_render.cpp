// bench_render.cpp —— 实时渲染管线基线
//
// 度量 `app::render_once`（Canvas 绘制 + Renderer 增量渲染成字节流）在典型终端图表
// 尺寸下的耗时。关注点：尺寸翻倍时耗时增长是否接近线性（光栅化 O(像素)、diff O(像素)，
// 不该出现平方级行为）。复用既有 `app::render_once`，不另造渲染入口。
#include <cstddef>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "bench.hpp"
#include "termchart/app/headless.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"

namespace {

using namespace termchart;

// 在画布上画一条正弦波 + 标题文字（与 examples/39 同款绘制调用）。
void draw_wave(canvas::Canvas& c, const char* tag) {
    c.draw_text(Point{0.0, 0.0}, std::string("bench render: ") + tag,
                Style{}.set_fg(Color::ansi16(7)));
    const Rect area{1.0, 2.0, static_cast<double>(c.width()) - 3.0,
                    static_cast<double>(c.height()) - 4.0};
    const Style data = Style{}.set_fg(Color::ansi16(2));
    const auto map = [&](double x, double y) {
        const double nx = x / 60.0;
        const double ny = (y + 1.0) / 2.0;  // y ∈ [-1, 1] → [0, 1]
        return Point{area.x + nx * (area.w - 1.0), area.y + (1.0 - ny) * (area.h - 1.0)};
    };
    for (int i = 0; i < 120; ++i) {
        const double x0 = static_cast<double>(i) / 2.0;
        const double x1 = static_cast<double>(i + 1) / 2.0;
        const double y0 = 0.8 * std::sin(0.5 * x0);
        const double y1 = 0.8 * std::sin(0.5 * x1);
        c.draw_line(map(x0, y0), map(x1, y1), U'\u2022', data);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const bool quick = bench::is_quick(argc, argv);
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> sizes =
        quick ? std::vector<std::pair<std::uint32_t, std::uint32_t>>{{80u, 24u}}
              : std::vector<std::pair<std::uint32_t, std::uint32_t>>{
                    {80u, 24u}, {120u, 40u}, {200u, 60u}};
    const int rounds = quick ? 3 : 20;

    bench::header("bench_render", "app::render_once（Canvas 绘制 + Renderer 字节流）");
    for (const auto& s : sizes) {
        const std::string size = std::to_string(s.first) + "x" + std::to_string(s.second);
        const double ns = bench::median_ns(
            [&] {
                (void)app::render_once(
                    s.first, s.second,
                    [&](canvas::Canvas& c) { draw_wave(c, "frame"); });
            },
            rounds);
        bench::report("render_once", size, ns);
    }
    return 0;
}
