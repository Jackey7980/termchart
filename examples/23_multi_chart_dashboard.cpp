// 23_multi_chart_dashboard.cpp —— 多图联动仪表盘：多个面板共享同一个 Viewport
//
// 参照 22_zoom_pan（interaction::Viewport 联动）与 10_gauge（子画布 blit 布局）：
//   * 左面板：折线主图，按共享 Viewport 取景；
//   * 右面板：全量概览 + 可见窗口标记（联动关系在这里可读）；
//   * 脚本化对共享 Viewport 做缩放/平移，两个面板同帧联动更新。
// 无交互、无真实时钟：输出确定，CI 可直接跑完。
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/interaction/zoom.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

constexpr double kX0 = 0.0;
constexpr double kX1 = 60.0;
constexpr double kY0 = -1.4;
constexpr double kY1 = 1.4;

// 确定性信号：三个不同频率的正弦叠加（与 22_zoom_pan 同源，便于对照）。
std::vector<Point> make_series() {
    std::vector<Point> points;
    points.reserve(240);
    for (int i = 0; i < 240; ++i) {
        const double x = static_cast<double>(i) / 4.0;
        const double y = 0.55 * std::sin(1.7 * x) + 0.30 * std::sin(5.1 * x + 1.0) +
                         0.15 * std::cos(11.0 * x);
        points.push_back(Point{x, y});
    }
    return points;
}

// 数据坐标 → 屏幕格（y 轴翻转：数据 y 向上、屏幕行向下）。
Point to_screen(Point data, const Rect& area, const interaction::Viewport& vp) {
    const double nx = vp.normalized_x(data.x);
    const double ny = vp.normalized_y(data.y);
    return Point{area.x + nx * (area.w - 1.0), area.y + (1.0 - ny) * (area.h - 1.0)};
}

void draw_box(canvas::Canvas& c, const Style& s) {
    const double w = static_cast<double>(c.width()) - 1.0;
    const double h = static_cast<double>(c.height()) - 1.0;
    c.draw_line(Point{0.0, 0.0}, Point{w, 0.0}, U'\u2500', s);
    c.draw_line(Point{0.0, h}, Point{w, h}, U'\u2500', s);
    c.draw_line(Point{0.0, 0.0}, Point{0.0, h}, U'\u2502', s);
    c.draw_line(Point{w, 0.0}, Point{w, h}, U'\u2502', s);
}

void draw_main_panel(canvas::Canvas& c, const std::vector<Point>& series,
                     const interaction::Viewport& vp) {
    const Style border = Style{}.set_fg(Color::ansi16(8));
    const Style data = Style{}.set_fg(Color::ansi16(2));
    draw_box(c, border);
    const double h = static_cast<double>(c.height());
    const Rect area{1.0, 1.0, static_cast<double>(c.width()) - 3.0, h - 5.0};
    c.draw_text(Point{1.0, h - 3.0}, "main: shared viewport", Style{}.set_fg(Color::ansi16(7)));

    // 与 22 同一条简化规则：两端都在同一侧界外的段整段跳过，其余交给 canvas 裁剪。
    const Rect visible = vp.visible();
    const auto out_x = [&visible](const Point& p, bool left) {
        return left ? p.x < visible.x : p.x > visible.right();
    };
    for (std::size_t i = 0; i + 1 < series.size(); ++i) {
        const Point& a = series[i];
        const Point& b = series[i + 1];
        const bool x_out = (out_x(a, true) && out_x(b, true)) ||
                           (out_x(a, false) && out_x(b, false));
        const bool y_out = (a.y < visible.y && b.y < visible.y) ||
                           (a.y > visible.bottom() && b.y > visible.bottom());
        if (x_out || y_out) {
            continue;
        }
        c.draw_line(to_screen(a, area, vp), to_screen(b, area, vp), U'\u2022', data);
    }
}

void draw_overview_panel(canvas::Canvas& c, const std::vector<Point>& series,
                         const interaction::Viewport& vp) {
    const Style border = Style{}.set_fg(Color::ansi16(8));
    const Style data = Style{}.set_fg(Color::ansi16(6));
    const Style window = Style{}.set_fg(Color::ansi16(3));
    draw_box(c, border);
    const double h = static_cast<double>(c.height());
    const Rect area{1.0, 1.0, static_cast<double>(c.width()) - 3.0, h - 5.0};
    c.draw_text(Point{1.0, h - 3.0}, "overview: full domain", Style{}.set_fg(Color::ansi16(7)));

    const auto to_over = [&](Point p) {
        const double nx = (p.x - kX0) / (kX1 - kX0);
        const double ny = (p.y - kY0) / (kY1 - kY0);
        return Point{area.x + nx * (area.w - 1.0), area.y + (1.0 - ny) * (area.h - 1.0)};
    };
    for (std::size_t i = 0; i + 1 < series.size(); ++i) {
        c.draw_line(to_over(series[i]), to_over(series[i + 1]), U'\u2022', data);
    }
    // 可见窗口标记：把共享视口的可见矩形描进概览。
    const Rect visible = vp.visible();
    const Point tl = to_over(Point{visible.x, visible.bottom()});
    const Point br = to_over(Point{visible.right(), visible.y});
    c.draw_line(tl, Point{br.x, tl.y}, U'\u2500', window);
    c.draw_line(Point{tl.x, br.y}, br, U'\u2500', window);
    c.draw_line(tl, Point{tl.x, br.y}, U'\u2502', window);
    c.draw_line(Point{br.x, tl.y}, br, U'\u2502', window);
}

// 子画布 blit（10_gauge 同款）：跳过空格，避免覆盖主画布已有内容。
void blit(canvas::Canvas& dst, const canvas::Canvas& src, std::uint32_t ox, std::uint32_t oy) {
    for (std::uint32_t y = 0; y < src.height(); ++y) {
        for (std::uint32_t x = 0; x < src.width(); ++x) {
            const auto& cell = src.at(x, y);
            if (cell.cp != U' ') {
                dst.put(ox + x, oy + y, cell.cp, cell.style);
            }
        }
    }
}

}  // namespace

int main() {
    const std::vector<Point> series = make_series();
    render::Renderer renderer;
    bool ok = true;

    interaction::Viewport vp(Rect{kX0, kY0, kX1 - kX0, kY1 - kY0}, 1e-6, /*keep_aspect=*/false);

    const auto emit = [&](const char* caption) {
        canvas::Canvas main(78, 22);
        main.draw_text(Point{0.0, 0.0}, "multi-chart dashboard: two panels share one Viewport",
                       Style{}.set_fg(Color::ansi16(7)));
        canvas::Canvas left(37, 18);
        draw_main_panel(left, series, vp);
        blit(main, left, 0, 1);
        canvas::Canvas right(37, 18);
        draw_overview_panel(right, series, vp);
        blit(main, right, 41, 1);

        char readout[128] = {};
        const Rect visible = vp.visible();
        std::snprintf(readout, sizeof(readout),
                      "zoom x%.2f   visible x[%.1f, %.1f]  y[%.2f, %.2f]",
                      vp.scale_x(), visible.x, visible.right(), visible.y, visible.bottom());
        main.draw_text(Point{0.0, 20.0}, readout, Style{}.set_fg(Color::ansi16(8)));
        main.draw_text(Point{0.0, 21.0},
                       "scripted: center zoom 2x, then pan right 10% (both panels follow)",
                       Style{}.set_fg(Color::ansi16(8)));

        if (api::write_stdout(std::string("\n== ") + caption + " ==\n") != ErrorCode::Ok) {
            ok = false;
        }
        if (api::write_stdout(renderer.render_full(main.to_buffer())) != ErrorCode::Ok) {
            ok = false;
        }
    };

    emit("initial: full domain visible");
    vp.zoom_at(2.0, vp.visible().center());
    emit("after center zoom 2x: both panels updated from the same Viewport");
    vp.pan_by((kX1 - kX0) * 0.1, 0.0);
    emit("after pan right 10%: overview window follows");

    api::flush_stdout();
    return ok ? 0 : 1;
}
