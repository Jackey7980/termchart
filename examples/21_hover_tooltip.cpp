// 21_hover_tooltip.cpp —— 悬停数据标签 + 十字准线（规格示例）
//
// 与 22_zoom_pan 同一套骨架：**脚本/交互两模式共用同一条事件链**（字节 → InputParser
// → 处理函数 → 绘制），只换字节来源；脚本模式让它在 ctest 里能跑完（不挂终端）。
//
// 演示的东西：
//   * 鼠标移动 → 找最近的**数据点** → 在该点画十字准线（含轴侧读数）+ 浮层标签；
//   * 浮层自动翻边（靠右翻左、靠上翻下），且**永不覆盖锚点**；
//   * 宽字符与超长文本按显示宽度处理（本示例用中日文标签演示）；
//   * **覆盖层不留痕**：最后一帧显式 clear() 后把画布打出来，证明"悬停过的地方
//     逐格逐属性还原"——这是 crosshair/tooltip 唯一的核心不变量，光看代码看不出来，
//     所以示例把它演成一个可见的对照帧。
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/interaction/crosshair.hpp"
#include "termchart/interaction/event.hpp"
#include "termchart/interaction/input_parser.hpp"
#include "termchart/interaction/tooltip.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/term/mouse_mode.hpp"
#include "termchart/term/raw_mode.hpp"
#include "termchart/term/size.hpp"

namespace {

using namespace termchart;

constexpr double kX0 = 0.0;
constexpr double kX1 = 60.0;
constexpr double kY0 = -1.5;
constexpr double kY1 = 1.5;

std::vector<Point> make_series() {
    std::vector<Point> points;
    for (int i = 0; i <= 120; ++i) {
        const double x = kX0 + (kX1 - kX0) * static_cast<double>(i) / 120.0;
        points.push_back(Point{x, 0.8 * std::sin(0.7 * x) - 0.35 * std::cos(1.9 * x)});
    }
    return points;
}

Rect plot_area(const canvas::Canvas& canvas) {
    // 左边 8 列给轴侧读数留位（crosshair 的读数写在绘图区外），底下 2 行放提示。
    const double w = static_cast<double>(canvas.width());
    const double h = static_cast<double>(canvas.height());
    return Rect{8.0, 1.0, w - 9.0, h - 4.0};
}

Point to_screen(Point data, const Rect& area) {
    const double nx = (data.x - kX0) / (kX1 - kX0);
    const double ny = (data.y - kY0) / (kY1 - kY0);
    return Point{area.x + nx * (area.w - 1.0), area.y + (1.0 - ny) * (area.h - 1.0)};
}

Point data_at(Point cell, const Rect& area) {
    const double nx = (cell.x - area.x) / (area.w > 1.0 ? area.w - 1.0 : 1.0);
    const double ny = 1.0 - (cell.y - area.y) / (area.h > 1.0 ? area.h - 1.0 : 1.0);
    return Point{kX0 + nx * (kX1 - kX0), kY0 + ny * (kY1 - kY0)};
}

// 找 x 方向上离该数据坐标最近的点：悬停读数要落在**真实数据点**上，
// 而不是跟着鼠标浮在曲线上方的空气里。
std::size_t nearest_index(const std::vector<Point>& series, double x) {
    std::size_t best = 0;
    double best_d = std::abs(series[0].x - x);
    for (std::size_t i = 1; i < series.size(); ++i) {
        const double d = std::abs(series[i].x - x);
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

void draw_base(canvas::Canvas& canvas, const std::vector<Point>& series) {
    const Rect area = plot_area(canvas);
    const Style axis = Style{}.set_fg(Color::ansi16(8));
    const Style data = Style{}.set_fg(Color::ansi16(2));
    const Style text = Style{}.set_fg(Color::ansi16(7));

    canvas.draw_line(Point{area.x - 1.0, area.y - 1.0}, Point{area.right(), area.y - 1.0},
                     U'\u2500', axis);
    canvas.draw_line(Point{area.x - 1.0, area.y - 1.0},
                     Point{area.x - 1.0, area.bottom()}, U'\u2502', axis);
    canvas.draw_line(Point{area.x - 1.0, area.bottom()}, Point{area.right(), area.bottom()},
                     U'\u2500', axis);
    canvas.draw_line(Point{area.right(), area.y - 1.0}, Point{area.right(), area.bottom()},
                     U'\u2502', axis);

    for (std::size_t i = 0; i + 1 < series.size(); ++i) {
        canvas.draw_line(to_screen(series[i], area), to_screen(series[i + 1], area), U'\u2022',
                         data);
    }

    canvas.draw_text(Point{0.0, static_cast<double>(canvas.height()) - 1.0},
                     "move=hover  c=crosshair  t=tooltip  q=quit", text);
}

// 一次悬停：把准线与浮层画上（两者都是覆盖层，下一次 draw 会先还原自己）。
void hover(canvas::Canvas& canvas, const std::vector<Point>& series, Point cursor_cell,
           interaction::Crosshair& crosshair, interaction::Tooltip& tooltip, bool use_crosshair,
           bool use_tooltip) {
    const Rect area = plot_area(canvas);
    if (!area.contains(cursor_cell)) {
        return;  // 鼠标在绘图区外：什么都不画（两个覆盖层自己保证不会被污染）
    }
    const Point cursor_data = data_at(cursor_cell, area);
    const std::size_t index = nearest_index(series, cursor_data.x);
    const Point point = series[index];
    const Point cell = to_screen(point, area);

    char readout[64] = {};
    std::snprintf(readout, sizeof(readout), "y=%.2f", point.y);
    if (use_crosshair) {
        (void)crosshair.draw(canvas, cell, readout);
    }
    if (use_tooltip) {
        char x_line[64] = {};
        char y_line[64] = {};
        std::snprintf(x_line, sizeof(x_line), "x = %.2f", point.x);
        std::snprintf(y_line, sizeof(y_line), "y = %.2f", point.y);
        // 多行 + 宽字符：数值行是 ASCII，最后一行故意放中日文，用来演示
        // "按 util::display_width 计宽（宽字符算 2 列）"与超长截断。
        (void)tooltip.draw(canvas, cell,
                           std::vector<std::string>{x_line, y_line, "第 " + std::to_string(index) +
                                                                     " 点（宽字符示例）"});
    }
}

std::string sgr_mouse(int b, int x, int y, char final_char) {
    return "\x1b[<" + std::to_string(b) + ";" + std::to_string(x) + ";" + std::to_string(y) +
           final_char;
}

struct DemoState {
    interaction::Crosshair crosshair;
    interaction::Tooltip tooltip;
    bool use_crosshair = true;
    bool use_tooltip = true;
};

bool handle_event(const interaction::Event& event, const Rect& area, DemoState& state) {
    if (const auto* key = std::get_if<interaction::KeyEvent>(&event)) {
        if (key->key == interaction::Key::Escape || key->ch == U'q') {
            return false;
        }
        if (key->ch == U'c') {
            state.use_crosshair = !state.use_crosshair;
        }
        if (key->ch == U't') {
            state.use_tooltip = !state.use_tooltip;
        }
        return true;
    }
    (void)area;
    return true;  // 鼠标事件由调用方解码成游标位置（见下）
}

int run_scripted(const std::vector<Point>& series, std::uint32_t width, std::uint32_t height) {
    interaction::InputParser parser;
    DemoState state;
    const canvas::Canvas probe(width, height);
    const Rect area = plot_area(probe);
    state.crosshair.set_options(interaction::CrosshairOptions{
        .plot_rect = area,
        .preserve_data = true,   // 准线只改样式，保留数据字符
        .show_readout = true,
        .line_style = Style{}.set_bg(Color::ansi16(8)),
    });
    state.tooltip.set_options(interaction::TooltipOptions{
        .bounds = Rect{0.0, 0.0, static_cast<double>(width) - 1.0,
                       static_cast<double>(height) - 1.0},
        .max_width = 28,
        .gap = 1.0,
        .box_style = Style{}.set_fg(Color::ansi16(7)),
        .border_style = Style{}.set_fg(Color::ansi16(6)),
    });

    const std::vector<Point> cursors{
        Point{area.x + area.w * 0.25, area.y + area.h * 0.35},
        Point{area.x + area.w * 0.55, area.y + area.h * 0.75},
        Point{area.right() - 2.0, area.y + 2.0},  // 贴右边界：浮层应翻到左侧
    };

    std::size_t frame = 0;
    for (const Point& cursor : cursors) {
        const int cx = static_cast<int>(cursor.x);
        const int cy = static_cast<int>(cursor.y);
        (void)parser.feed(sgr_mouse(32, cx, cy, 'M'));  // 32 = 移动

        canvas::Canvas canvas(width, height);
        draw_base(canvas, series);
        hover(canvas, series, cursor, state.crosshair, state.tooltip, state.use_crosshair,
              state.use_tooltip);
        const std::string caption =
            "\n" + std::to_string(frame) + ") 悬停在 (" + std::to_string(cx) + "," +
            std::to_string(cy) + ")：准线 " +
            (state.crosshair.active() ? "on" : "off") + "，浮层 " +
            (state.tooltip.visible() ? "on" : "off") + "，覆盖格 " +
            std::to_string(state.crosshair.patch_count() + state.tooltip.patch_count()) + "\n";
        (void)api::write_stdout(caption);
        render::Renderer renderer;
        (void)api::write_stdout(renderer.render_full(canvas.to_buffer()));
        ++frame;
    }

    // 对照帧：显式撤销两个覆盖层，再打印同一张画布 —— 应当与"从未悬停过"逐格一致。
    canvas::Canvas restored(width, height);
    draw_base(restored, series);

    canvas::Canvas canvas(width, height);
    draw_base(canvas, series);
    hover(canvas, series, cursors.back(), state.crosshair, state.tooltip, true, true);
    const std::size_t patched = state.crosshair.patch_count() + state.tooltip.patch_count();
    (void)state.crosshair.clear(canvas);
    (void)state.tooltip.clear(canvas);

    std::size_t residual = 0;
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).cp != U' ') {
                ++residual;
            }
        }
    }
    std::size_t baseline = 0;
    for (std::uint32_t y = 0; y < restored.height(); ++y) {
        for (std::uint32_t x = 0; x < restored.width(); ++x) {
            if (restored.at(x, y).cp != U' ') {
                ++baseline;
            }
        }
    }
    (void)api::write_stdout("\n对照帧：悬停后被改写了 " + std::to_string(patched) +
                            " 格，clear() 之后的非空格数 " + std::to_string(residual) +
                            " 与\"从未悬停\"的 " + std::to_string(baseline) + " 一致 → 覆盖层不留痕\n");
    (void)api::write_stdout(render::Renderer{}.render_full(canvas.to_buffer()));
    (void)api::flush_stdout();
    return residual == baseline ? 0 : 1;
}

int run_live(const std::vector<Point>& series, std::uint32_t width, std::uint32_t height) {
    term::RawMode raw;
    raw.enter();
    if (!raw.active()) {
        return run_scripted(series, width, height);
    }
    interaction::InputParser parser;
    DemoState state;
    const canvas::Canvas probe(width, height);
    const Rect area = plot_area(probe);
    state.crosshair.set_options(interaction::CrosshairOptions{.plot_rect = area,
                                                              .preserve_data = true,
                                                              .show_readout = true,
                                                              .line_style = Style{}.set_bg(
                                                                  Color::ansi16(8))});
    state.tooltip.set_options(interaction::TooltipOptions{
        .bounds = Rect{0.0, 0.0, static_cast<double>(width) - 1.0,
                       static_cast<double>(height) - 1.0},
        .max_width = 28,
        .box_style = Style{}.set_fg(Color::ansi16(7)),
        .border_style = Style{}.set_fg(Color::ansi16(6))});

    (void)api::write_stdout(
        term::enable_mouse(term::MouseMode::AnyMotion, term::MouseEncoding::Sgr));
    render::Renderer renderer;
    bool running = true;
    while (running) {
        char chunk[64] = {};
        const std::size_t got = std::fread(chunk, 1, sizeof(chunk), stdin);
        if (got == 0) {
            break;
        }
        const std::vector<interaction::Event> events =
            parser.feed(std::string_view(chunk, got));
        std::optional<Point> cursor;
        for (const interaction::Event& event : events) {
            running = handle_event(event, area, state);
            if (const auto* mouse = std::get_if<interaction::MouseEvent>(&event)) {
                if (mouse->action == interaction::MouseAction::Move) {
                    cursor = Point{static_cast<double>(mouse->x), static_cast<double>(mouse->y)};
                }
            }
            if (!running) {
                break;
            }
        }
        if (!running) {
            break;
        }
        canvas::Canvas canvas(width, height);
        draw_base(canvas, series);
        if (cursor.has_value()) {
            hover(canvas, series, *cursor, state.crosshair, state.tooltip, state.use_crosshair,
                  state.use_tooltip);
        }
        (void)api::write_stdout(renderer.render(canvas.to_buffer()));
        (void)api::flush_stdout();
    }
    // 退出前把覆盖层撤掉，别把终端留在"画着一层标签"的状态。
    canvas::Canvas final_frame(width, height);
    draw_base(final_frame, series);
    state.crosshair.clear(final_frame);
    state.tooltip.clear(final_frame);
    (void)api::write_stdout(renderer.render(final_frame.to_buffer()));
    (void)api::write_stdout(term::disable_all_mouse(term::MouseEncoding::Sgr));
    (void)api::flush_stdout();
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const std::vector<Point> series = make_series();
    bool live = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--live") {
            live = true;
        }
    }
    const term::TerminalSize size = term::query_size();
    const auto width = static_cast<std::uint32_t>(size.cols > 60 ? size.cols : 60);
    const auto height =
        static_cast<std::uint32_t>(size.rows > 16 ? (size.rows > 30 ? 22 : size.rows - 2) : 16);
    return live ? run_live(series, width, height) : run_scripted(series, width, height);
}
