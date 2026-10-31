// examples/78_chart_scene.cpp —— 图表的**真实 Scene** 通路 + 动画后处理
//
// 为什么值得单独一个示例：以前"图表"只会往字符格里落笔，想要矢量导出就得先把
// Canvas 逐格抄成 TextOp——那只是给位图换了个容器，缩放一次就糊。现在 chart 直接
// 产出 Scene（MoveTo/LineTo/StrokeOp/RectOp/TextOp/GroupOp）：
//
//   1. 终端：render_scene 把几何光栅成盲文/半块字符（本示例打印的部分）；
//   2. 矢量：SvgBackend 输出 <path>/<rect>，无损缩放（写到 chart_scene.svg）；
//   3. 交互：GroupOp 把每条系列圈成一组，命中测试按组收拢。
//
// 动画是**后处理**（chart/scene_animation.hpp）：`to_scene` 只出完整几何，
// `apply_animation` 按进度改写组内图元——折线从左向右揭开、柱子从基线长高，
// 轴/网格/标题/图例一个字节都不动。于是同一份基准 Scene 能反复出帧，
// 不必每帧重算轴与刻度。
//
// 用法：
//   78_chart_scene [output.svg] [--svg-dir=DIR] [--realtime]
//   * 默认按固定步长跑 1 秒时间线（确定性，便于快照/回归）；
//   * --realtime 用 steady_clock 真等 1 秒并原地重绘（给人看）；
//   * --svg-dir=DIR 额外把**每一帧**写成 SVG。
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "termchart/api/chart.hpp"
#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/scene_renderer.hpp"
#include "termchart/chart/cartesian/bar_chart.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/util/easing.hpp"

namespace {

using namespace termchart;

constexpr std::uint32_t kCols = 62;
constexpr std::uint32_t kRows = 16;

// 两条线的动画错开 0.15s 起跑：能看出"逐条揭开"，而不是一整块闪出来。
chart::LineChart make_lines() {
    chart::LineChart chart;
    chart.set_title("真实 Scene 通路");

    chart::Series revenue;
    revenue.name = "收入";
    revenue.color = Color::ansi16(2);
    revenue.points = {{0.0, 3.0}, {1.0, 5.0}, {2.0, 4.0}, {3.0, 7.0},
                      {4.0, 6.0}, {5.0, 9.0}, {6.0, 8.0}};

    chart::Series cost;
    cost.name = "成本";
    cost.color = Color::ansi16(1);
    cost.points = {{0.0, 2.0}, {1.0, 3.0}, {2.0, 3.0}, {3.0, 4.0},
                   {4.0, 4.0}, {5.0, 5.0}, {6.0, 5.0}};

    chart.add_animation("收入", 0.6, util::Easing::EaseOutCubic, 0.0);
    chart.add_animation("成本", 0.6, util::Easing::EaseOutCubic, 0.15);
    chart.add_series(std::move(revenue));
    chart.add_series(std::move(cost));
    return chart;
}

// 含负值：验证"从基线长高"对正负柱是同一套公式（顶边/底边贴 0 线）。
chart::BarChart make_bars() {
    chart::BarChart bar;
    bar.set_values({4.0, 7.0, 3.0, -2.0, 5.0},
                   {"一", "二", "三", "四", "五"}, Color::ansi16(4), "销量");
    bar.add_animation("销量", 0.8, util::Easing::EaseOutQuad, 0.0);
    return bar;
}

struct Census {
    std::size_t total = 0;
    std::size_t groups = 0;
    std::size_t segments = 0;  // MoveTo + LineTo
    std::size_t shapes = 0;    // Rect + Arc
    std::size_t texts = 0;
};

Census census(const scene::Scene& scene) {
    Census c;
    for (const auto& command : scene.commands()) {
        ++c.total;
        switch (scene::kind_of(command)) {
            case scene::CommandKind::Group:
                ++c.groups;
                break;
            case scene::CommandKind::MoveTo:
            case scene::CommandKind::LineTo:
                ++c.segments;
                break;
            case scene::CommandKind::Rect:
            case scene::CommandKind::Arc:
                ++c.shapes;
                break;
            case scene::CommandKind::Text:
                ++c.texts;
                break;
            case scene::CommandKind::Fill:
            case scene::CommandKind::Stroke:
            case scene::CommandKind::Image:
                break;
            default: 
                break;
        }
    }
    return c;
}

// 一帧 = 取当前时刻的 Scene + 光栅到字符格 + 打印。
// 每帧新建 Renderer：render_full 自带 prev 重置，跨帧复用只会让第二帧开始
// 走增量 diff（输出里全是光标定位，不再是可读的整帧）。
bool draw_frame(const chart::Chart& chart, const char* label, double t) {
    const auto frame = api::chart_to_scene(chart, kCols, kRows);
    if (!frame.has_value()) {
        std::fprintf(stderr, "chart_to_scene failed: %s\n", to_string(frame.error()));
        return false;
    }
    std::printf("\n── %s  t=%.2fs ──\n", label, t);
    std::fflush(stdout);

    canvas::Canvas canvas(kCols, kRows);
    canvas::render_scene(*frame, canvas);
    render::Renderer renderer;
    const bool ok = api::write_stdout(renderer.render_full(canvas.to_buffer())) ==
                    ErrorCode::Ok;
    api::flush_stdout();
    return ok;
}

bool write_svg(const chart::Chart& chart, const std::string& path) {
    const auto svg = api::chart_to_svg(chart, kCols, kRows);
    if (!svg.has_value()) {
        std::fprintf(stderr, "chart_to_svg failed: %s\n", to_string(svg.error()));
        return false;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "cannot open %s\n", path.c_str());
        return false;
    }
    out.write(svg->data(), static_cast<std::streamsize>(svg->size()));
    return true;
}

// 跑一条时间线：帧数固定 → 输出与快照确定性成立；--realtime 只是把帧间隔
// 变成真实等待（并原地重绘），不影响任何一帧的内容。
template <typename ChartT, typename LabelFn>
bool run_timeline(ChartT& chart, int frames, double total_seconds, const std::string& svg_dir,
                  const std::string& svg_prefix, bool realtime, LabelFn label_of) {
    bool ok = true;
    const auto begin = std::chrono::steady_clock::now();
    for (int i = 0; i < frames; ++i) {
        const double t = total_seconds * static_cast<double>(i) / (frames - 1);
        chart.tick(t);  // 绝对时刻：暂停/续播只要改传时刻，不必维护 dt
        if (realtime && i > 0) {
            const auto target =
                begin + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                            std::chrono::duration<double>(t));
            std::this_thread::sleep_until(target);
            // 回到本帧顶部，原地覆盖上一帧（非 tty 时这串 SGR 只是噪声，无副作用）。
            std::printf("\x1b[%uA", kRows + 2);
        }
        ok = draw_frame(chart, label_of(i, t), t) && ok;
        if (!svg_dir.empty()) {
            char name[64];
            std::snprintf(name, sizeof(name), "/%s_%02d.svg", svg_prefix.c_str(), i);
            ok = write_svg(chart, svg_dir + name) && ok;
        }
    }
    return ok;
}

}  // namespace

int main(int argc, char** argv) {
    // 默认把终帧 SVG 写进系统临时目录（不污染工作树）：要落到固定位置就传
    // `78_chart_scene out.svg`。多帧 SVG 用 `--svg-dir=DIR`。
    std::string path = (std::filesystem::temp_directory_path() / "chart_scene.svg").string();
    std::string svg_dir;
    bool realtime = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--realtime") {
            realtime = true;
        } else if (arg.rfind("--svg-dir=", 0) == 0) {
            svg_dir = arg.substr(10);
        } else {
            path = arg;
        }
    }

    bool ok = true;

    // ── ① 静态一帧：证明"图表 → 真实 Scene"本身成立 ──────────────────
    const chart::LineChart static_lines = make_lines();
    const auto scene = api::chart_to_scene(static_lines, kCols, kRows);
    if (!scene.has_value()) {
        std::fprintf(stderr, "chart_to_scene failed: %s\n", to_string(scene.error()));
        return 1;
    }
    const Census c = census(*scene);
    std::printf("scene: %zu 指令（%zu 分组 / %zu 折线段 / %zu 形状 / %zu 文本）\n",
                c.total, c.groups / 2, c.segments, c.shapes, c.texts);

    // ── ② 折线：从左向右揭开（两条线错开起跑） ────────────────────────
    chart::LineChart lines = make_lines();
    constexpr int kLineFrames = 5;  // 0 / .25 / .5 / .75 / 1
    ok = run_timeline(
             lines, kLineFrames, 1.0, svg_dir, "lines", realtime,
             [](int i, double) {
                 static char text[64];
                 std::snprintf(text, sizeof(text), "折线动画 %d/%d（从左向右）", i + 1,
                               kLineFrames);
                 return text;
             }) &&
         ok;

    // ── ③ 柱状：从基线长高（含负值柱，向下长） ────────────────────────
    chart::BarChart bars = make_bars();
    constexpr int kBarFrames = 4;  // 0 / .33 / .66 / 1
    ok = run_timeline(
             bars, kBarFrames, 1.0, svg_dir, "bars", realtime,
             [](int i, double) {
                 static char text[64];
                 std::snprintf(text, sizeof(text), "柱状动画 %d/%d（从基线长高）", i + 1,
                               kBarFrames);
                 return text;
             }) &&
         ok;

    // ── ④ 矢量消费者：终帧 Scene → SVG（折线是 <path>，柱是 <rect>） ──
    ok = write_svg(lines, path) && ok;
    if (!svg_dir.empty()) {
        ok = write_svg(bars, svg_dir + "/bars_final.svg") && ok;
    }
    std::printf("\nSVG 已写入 %s（终帧）\n", path.c_str());

    return ok ? 0 : 1;
}
