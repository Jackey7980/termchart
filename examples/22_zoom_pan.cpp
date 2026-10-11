// 22_zoom_pan.cpp —— 滚轮缩放 + 拖拽平移（规格示例）
//
// 两种模式，**共用同一条事件处理链**（只换字节来源）：
//   * 脚本模式（默认；stdin 不是 tty 时自动进入）：把一串**合成的 SGR 鼠标字节**
//     喂给同一个 InputParser，打印若干帧后退出 —— 这样它能在 CI/ctest 里跑完，
//     而不是挂在终端里等人操作；
//   * 交互模式（stdin 是 tty 且给了 `--live`）：进原始模式 + 打开 SGR 鼠标上报，
//     滚轮缩放、按住左键拖拽平移，q / Esc 退出（析构时恢复终端）。
//
// 为什么脚本模式也走"字节 → 解析器"而不是直接构造事件对象：这样两条路径的差异
// 只有"字节从哪来"，解析、视口、手势、绘制全都同一份代码；顺带把 SGR 编码也验了。
//
// 语义要点（都由 interaction::Viewport / PanGesture 保证，示例只负责接线）：
//   * 缩放锚点 = 鼠标所在的**数据坐标**，缩放后仍停在同一个格（夹取介入时让步）；
//   * 拖拽 = "内容跟随指针"：指针往右下走，视口反向移动，贴边即停；
//   * 视口永远不越出数据界（不露界外空白）。
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
#include "termchart/interaction/event.hpp"
#include "termchart/interaction/input_parser.hpp"
#include "termchart/interaction/pan.hpp"
#include "termchart/interaction/zoom.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/term/mouse_mode.hpp"
#include "termchart/term/raw_mode.hpp"
#include "termchart/term/size.hpp"

namespace {

using namespace termchart;

constexpr double kZoomStep = 1.25;      // 每格滚轮的缩放倍率
constexpr std::size_t kSamples = 600;   // 数据点数
constexpr double kX0 = 0.0;
constexpr double kX1 = 60.0;
constexpr double kY0 = -1.4;
constexpr double kY1 = 1.4;

// 确定性信号：三个不同频率的正弦叠加，形状有细节但不依赖随机数。
std::vector<Point> make_series() {
    std::vector<Point> points;
    points.reserve(kSamples);
    for (std::size_t i = 0; i < kSamples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kSamples - 1);
        const double x = kX0 + t * (kX1 - kX0);
        const double y = 0.55 * std::sin(1.7 * x) + 0.30 * std::sin(5.1 * x + 1.0) +
                         0.15 * std::cos(11.0 * x);
        points.push_back(Point{x, y});
    }
    return points;
}

// 绘图区（左边一格画 y 轴、底下**两行**留给读数与提示，故高度再减 4）。
Rect plot_area(const canvas::Canvas& canvas) {
    const double w = static_cast<double>(canvas.width());
    const double h = static_cast<double>(canvas.height());
    return Rect{1.0, 1.0, w - 2.0, h - 4.0};
}

// 数据坐标 → 屏幕格（y 轴翻转：数据 y 向上、屏幕行向下）。
Point to_screen(Point data, const Rect& area, const interaction::Viewport& viewport) {
    const double nx = viewport.normalized_x(data.x);
    const double ny = viewport.normalized_y(data.y);
    return Point{area.x + nx * (area.w - 1.0), area.y + (1.0 - ny) * (area.h - 1.0)};
}

// 屏幕格 → 数据坐标（to_screen 的逆，缩放锚点与拖拽折算都要用）。
Point data_at(Point cell, const Rect& area, const interaction::Viewport& viewport) {
    const double nx = (cell.x - area.x) / (area.w > 1.0 ? area.w - 1.0 : 1.0);
    const double ny = 1.0 - (cell.y - area.y) / (area.h > 1.0 ? area.h - 1.0 : 1.0);
    const Rect visible = viewport.visible();
    return Point{visible.x + nx * visible.w, visible.y + ny * visible.h};
}

void draw_frame(canvas::Canvas& canvas, const std::vector<Point>& series,
                const interaction::Viewport& viewport) {
    const Rect area = plot_area(canvas);
    const Style axis = Style{}.set_fg(Color::ansi16(8));
    const Style data = Style{}.set_fg(Color::ansi16(2));
    const Style text = Style{}.set_fg(Color::ansi16(7));

    // 绘图区边框：只画外框，越界由 canvas 自己裁剪。
    canvas.draw_line(Point{area.x - 1.0, area.y - 1.0}, Point{area.right(), area.y - 1.0},
                     U'\u2500', axis);
    canvas.draw_line(Point{area.x - 1.0, area.y - 1.0},
                     Point{area.x - 1.0, area.bottom()}, U'\u2502', axis);
    canvas.draw_line(Point{area.x - 1.0, area.bottom()}, Point{area.right(), area.bottom()},
                     U'\u2500', axis);
    canvas.draw_line(Point{area.right(), area.y - 1.0}, Point{area.right(), area.bottom()},
                     U'\u2502', axis);

    // 数据折线：只用视口内的点，段两端的判断都按可见 x 范围做，避免画到界外又被裁掉。
    const Rect visible = viewport.visible();
    const auto outside_x = [&visible](const Point& p, bool left) {
        return left ? p.x < visible.x : p.x > visible.right();
    };
    for (std::size_t i = 0; i + 1 < series.size(); ++i) {
        const Point& a = series[i];
        const Point& b = series[i + 1];
        // 两端都在同一侧界外就整段跳过（x 与 y 都要判）：否则线段会从界外插值过来，
        // 在边框行上留下"点状涂抹"。只有一端在外时照画，让 canvas 去裁剪。
        const bool x_out = (outside_x(a, true) && outside_x(b, true)) ||
                           (outside_x(a, false) && outside_x(b, false));
        const bool y_out = (a.y < visible.y && b.y < visible.y) ||
                           (a.y > visible.bottom() && b.y > visible.bottom());
        if (x_out || y_out) {
            continue;
        }
        canvas.draw_line(to_screen(a, area, viewport), to_screen(b, area, viewport), U'\u2022',
                         data);
    }

    // 读数行 + 提示行。
    char readout[128] = {};
    std::snprintf(readout, sizeof(readout),
                  "zoom x%.2f   x[%.1f,%.1f]   y[%.2f,%.2f]   data %.0fx%.2f",
                  viewport.scale_x(), visible.x, visible.right(), visible.y,
                  visible.bottom(), visible.w, visible.h);
    canvas.draw_text(Point{0.0, static_cast<double>(canvas.height()) - 2.0}, readout, text);
    canvas.draw_text(Point{0.0, static_cast<double>(canvas.height()) - 1.0},
                     "wheel=zoom  drag=pan  q=quit", axis);
}

// 事件处理：脚本与交互两条路径共用。返回 false 表示请求退出。
bool handle_event(const interaction::Event& event, interaction::Viewport& viewport,
                  interaction::PanGesture& gesture, const Rect& area) {
    if (const auto* key = std::get_if<interaction::KeyEvent>(&event)) {
        if (key->key == interaction::Key::Escape || key->ch == U'q') {
            return false;
        }
        return true;
    }
    const auto* mouse = std::get_if<interaction::MouseEvent>(&event);
    if (mouse == nullptr) {
        return true;
    }
    const Point cell{static_cast<double>(mouse->x), static_cast<double>(mouse->y)};

    switch (mouse->action) {
        case interaction::MouseAction::ScrollUp:
            // 锚点是一个**数据坐标**：用它反推，缩放后该点仍落在同一个格上。
            viewport.zoom_at(kZoomStep, data_at(cell, area, viewport));
            break;
        case interaction::MouseAction::ScrollDown:
            viewport.zoom_at(1.0 / kZoomStep, data_at(cell, area, viewport));
            break;
        case interaction::MouseAction::Press:
            if (mouse->button == interaction::MouseButton::Left) {
                gesture.press(cell);
            }
            break;
        case interaction::MouseAction::Move: {
            const std::optional<Point> delta = gesture.drag_to(cell);
            if (delta.has_value()) {
                // 格位移 → 数据位移，注意两轴的符号**不一样**：
                //   * x：`pan_by` 正值 = 窗口右移 = 内容左移；要让内容跟随指针右移，
                //     窗口必须左移 → 取负号；
                //   * y：屏幕行向下增大、数据 y 向上增大，两者反向；"窗口下移"
                //     （y 减小）会让内容在屏幕上**上移**。要让内容跟随指针下移，
                //     窗口必须**上移**（y 增大）→ 这里不取负号。
                // （第一版两轴都取了负号，结果纵向拖拽把内容推向反方向——数字一看就露。）
                const double per_cell_x = viewport.visible().w / (area.w > 1.0 ? area.w - 1.0 : 1.0);
                const double per_cell_y = viewport.visible().h / (area.h > 1.0 ? area.h - 1.0 : 1.0);
                viewport.pan_by(-delta->x * per_cell_x, delta->y * per_cell_y);
            }
            break;
        }
        case interaction::MouseAction::Release:
            gesture.release();
            break;
    }
    return true;
}

// 合成一段 SGR 鼠标字节：b 是按钮/动作码，final 'M' 为按下类、'm' 为释放。
std::string sgr_mouse(int b, int x, int y, char final_char) {
    return "\x1b[<" + std::to_string(b) + ";" + std::to_string(x) + ";" + std::to_string(y) +
           final_char;
}

int run_scripted(const std::vector<Point>& series, std::uint32_t width, std::uint32_t height) {
    interaction::Viewport viewport(
        Rect{kX0, kY0, kX1 - kX0, kY1 - kY0}, 1e-6, /*keep_aspect=*/false);
    interaction::PanGesture gesture;
    interaction::InputParser parser;
    render::Renderer renderer;

    const canvas::Canvas probe(width, height);
    const Rect area = plot_area(probe);
    const int cx = static_cast<int>(area.x + area.w / 2.0);
    const int cy = static_cast<int>(area.y + area.h / 2.0);

    struct Step {
        std::string caption;
        std::string bytes;
    };
    const std::vector<Step> steps{
        {"0) 初始：整个数据界可见（zoom x1.00）", ""},
        {"1) 在绘图区中心向上滚 4 次：以该点为锚放大（锚点那格应不动）",
         sgr_mouse(64, cx, cy, 'M') + sgr_mouse(64, cx, cy, 'M') + sgr_mouse(64, cx, cy, 'M') +
             sgr_mouse(64, cx, cy, 'M')},
        {"2) 按住左键把内容往右下拖 8 格：视口反向平移（贴边即停）",
         sgr_mouse(0, cx, cy, 'M') + sgr_mouse(32, cx + 4, cy + 4, 'M') +
             sgr_mouse(32, cx + 8, cy + 8, 'M') + sgr_mouse(0, cx + 8, cy + 8, 'm')},
        {"3) 在中心向下滚 2 次：缩小（夹取会把视口拉回数据界内）",
         sgr_mouse(65, cx, cy, 'M') + sgr_mouse(65, cx, cy, 'M')},
    };

    for (const Step& step : steps) {
        const std::vector<interaction::Event> events = parser.feed(step.bytes);
        for (const interaction::Event& event : events) {
            if (!handle_event(event, viewport, gesture, area)) {
                break;
            }
        }
        canvas::Canvas canvas(width, height);
        draw_frame(canvas, series, viewport);
        (void)api::write_stdout("\n" + step.caption + "\n");
        (void)api::write_stdout(renderer.render_full(canvas.to_buffer()));
    }
    (void)api::flush_stdout();
    return 0;
}

int run_live(const std::vector<Point>& series, std::uint32_t width, std::uint32_t height) {
    interaction::Viewport viewport(Rect{kX0, kY0, kX1 - kX0, kY1 - kY0}, 1e-6, false);
    interaction::PanGesture gesture;
    interaction::InputParser parser;
    render::Renderer renderer;
    term::RawMode raw;
    raw.enter();
    if (!raw.active()) {
        return run_scripted(series, width, height);  // 拿不到 tty 就退回脚本模式
    }
    // 打开鼠标上报；离开时**必须**清场（终端鼠标模式是叠加的）。
    (void)api::write_stdout(term::enable_mouse(term::MouseMode::AnyMotion,
                                               term::MouseEncoding::Sgr));

    const canvas::Canvas probe(width, height);
    const Rect area = plot_area(probe);
    bool running = true;
    canvas::Canvas canvas(width, height);
    draw_frame(canvas, series, viewport);
    (void)api::write_stdout(renderer.render_full(canvas.to_buffer()));
    (void)api::flush_stdout();

    while (running) {
        char chunk[64] = {};
        const std::size_t got = std::fread(chunk, 1, sizeof(chunk), stdin);
        if (got == 0) {
            break;
        }
        const std::vector<interaction::Event> events =
            parser.feed(std::string_view(chunk, got));
        for (const interaction::Event& event : events) {
            running = handle_event(event, viewport, gesture, area);
            if (!running) {
                break;
            }
        }
        canvas::Canvas next(width, height);
        draw_frame(next, series, viewport);
        (void)api::write_stdout(renderer.render(next.to_buffer()));  // 增量：只输出变化格
        (void)api::flush_stdout();
    }

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
    const auto width = static_cast<std::uint32_t>(size.cols > 40 ? size.cols : 40);
    const auto height = static_cast<std::uint32_t>(size.rows > 12 ? (size.rows > 30 ? 22 : size.rows - 2) : 12);

    return live ? run_live(series, width, height) : run_scripted(series, width, height);
}
