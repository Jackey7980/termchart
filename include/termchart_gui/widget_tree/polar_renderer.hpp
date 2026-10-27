// widget_tree/polar_renderer.hpp —— 风玫瑰/极坐标折线（GUI 原生渲染插头）
//
// 把核心库里"字符化"的 polar（落 '•' 格子）换成像素级极坐标图：同心参考环 + 8 向
// 方位辐条（N/NE/.../NW，ASCII）+ 24h 风向(角度)×风速(半径) 数据折线/点 + 当前点高亮。
// 直接吐 Scene 指令，复用 ChartWidget 的动画/缓存/暗色主题。全部画在 base 图层，
// area/line 层留空；不做悬停（hit_index 恒 -1）。
#ifndef TERMCHART_GUI_WIDGET_TREE_POLAR_RENDERER_HPP
#define TERMCHART_GUI_WIDGET_TREE_POLAR_RENDERER_HPP

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart_gui/backend_tk/tk_canvas.hpp"
#include "termchart_gui/widget_tree/i_chart_renderer.hpp"

namespace termchart_gui {

class PolarRenderer final : public IChartRenderer {
public:
    // 每个点：x = 风向（度，气象约定 0=N,90=E）；y = 风速（m/s）。
    void set_points(std::vector<termchart::Point> pts) { pts_ = std::move(pts); }
    void set_title(std::string s) { title_ = std::move(s); }
    void set_rmax(double r) { rmax_ = r > 0.0 ? r : 0.0; }

    [[nodiscard]] bool empty() const override { return pts_.empty(); }

    [[nodiscard]] ChartGeom compute_geom(int W, int H) const override {
        ChartGeom g;
        g.n = 1;
        g.pl = 18.0;
        g.pt = 20.0;
        g.pr = static_cast<double>(W) - 14.0;
        g.pb = static_cast<double>(H) - 14.0;
        g.xs.resize(1);  // 维持 xs/ly 长度 == n 不变式（n=1）
        g.ly.resize(1);
        return g;
    }

    void build_base_scene(termchart::scene::SceneBuilder& b, const ChartGeom& g, double /*glow*/,
                          bool /*dragging*/) const override {
        const double cx = (g.pl + g.pr) / 2.0;
        const double cy = (g.pt + g.pb) / 2.0;
        const double R = std::min(g.pr - g.pl, g.pb - g.pt) / 2.0 * 0.74;
        const double rmax = rmax_ > 0.0 ? rmax_ : compute_rmax();

        // 同心参考环
        for (int k = 1; k <= 4; ++k) {
            const double r = R * static_cast<double>(k) / 4.0;
            stroke_path(b, Rgb{58, 63, 72}, 1.0,
                        [&] { b.arc(termchart::Point{cx, cy}, r, 0.0, 2.0 * kPI); });
        }
        // 8 向辐条
        const char* dirs[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
        for (int k = 0; k < 8; ++k) {
            const double a = (k * 45.0 - 90.0) * kPI / 180.0;
            stroke_path(b, Rgb{46, 50, 58}, 1.0, [&] {
                b.move_to(cx, cy);
                b.line_to(cx + R * std::cos(a), cy + R * std::sin(a));
            });
        }
        // 方位标签（环外）
        Style gray;
        gray.set_fg(termchart::Color::rgb(150, 152, 162));
        const double lr = R + 11.0;
        for (int k = 0; k < 8; ++k) {
            const double a = (k * 45.0 - 90.0) * kPI / 180.0;
            const std::string lab = dirs[k];
            b.text(termchart::Point{cx + lr * std::cos(a) - 4.0, cy + lr * std::sin(a) - 4.0}, lab,
                   gray);
        }

        // 数据：折线 + 点
        if (!pts_.empty()) {
            const auto col = termchart::Color::rgb(90, 170, 230);
            b.stroke(col, 2.0);
            bool first = true;
            for (const auto& p : pts_) {
                if (!std::isfinite(p.x) || !std::isfinite(p.y)) continue;
                const double a = (p.x - 90.0) * kPI / 180.0;
                const double rr = clamp01(p.y / rmax) * R;
                const double x = cx + rr * std::cos(a);
                const double y = cy + rr * std::sin(a);
                if (first) {
                    b.move_to(x, y);
                    first = false;
                } else {
                    b.line_to(x, y);
                }
            }
            b.stroke(col, 2.0);  // 收口
            for (const auto& p : pts_) {
                if (!std::isfinite(p.x) || !std::isfinite(p.y)) continue;
                const double a = (p.x - 90.0) * kPI / 180.0;
                const double rr = clamp01(p.y / rmax) * R;
                fill_path(b, Rgb{120, 190, 235}, [&] {
                    b.arc(termchart::Point{cx + rr * std::cos(a), cy + rr * std::sin(a)}, 2.5, 0.0,
                          2.0 * kPI);
                });
            }
            // 当前点（最后一个有限点）高亮
            int li = -1;
            for (int i = static_cast<int>(pts_.size()) - 1; i >= 0; --i) {
                if (std::isfinite(pts_[static_cast<std::size_t>(i)].x) &&
                    std::isfinite(pts_[static_cast<std::size_t>(i)].y)) {
                    li = i;
                    break;
                }
            }
            if (li >= 0) {
                const auto& p = pts_[static_cast<std::size_t>(li)];
                const double a = (p.x - 90.0) * kPI / 180.0;
                const double rr = clamp01(p.y / rmax) * R;
                const double x = cx + rr * std::cos(a);
                const double y = cy + rr * std::sin(a);
                stroke_path(b, Rgb{255, 210, 120}, 2.0,
                            [&] { b.arc(termchart::Point{x, y}, 5.0, 0.0, 2.0 * kPI); });
            }
        }

        // 标题
        Style white;
        white.set_fg(termchart::Color::rgb(232, 234, 240));
        b.text(termchart::Point{cx - static_cast<double>(title_.size()) * 4.5, g.pt - 4.0}, title_,
               white);
    }

    void build_area_scene(termchart::scene::SceneBuilder&, const ChartGeom&, double) const override {}
    void build_line_scene(termchart::scene::SceneBuilder&, const ChartGeom&, double) const override {}
    [[nodiscard]] int hit_index(const ChartGeom&, double, double) const override { return -1; }
    void draw_hover(TkCanvas&, const Rect&, const ChartGeom&, int, double) const override {}
    void draw_tooltip(TkCanvas&, const Rect&, const ChartGeom&, int) const override {}

private:
    static constexpr double kPI = 3.141592653589793;

    static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
    double compute_rmax() const {
        double m = 0.0;
        for (const auto& p : pts_) m = std::max(m, p.y);
        return m > 0.0 ? m * 1.15 : 1.0;
    }

    template <typename F>
    static void stroke_path(termchart::scene::SceneBuilder& b, Rgb col, double w, F&& build) {
        const auto c = termchart::Color::rgb(col.r, col.g, col.b);
        b.stroke(c, w);
        build();
        b.stroke(c, w);
    }
    template <typename F>
    static void fill_path(termchart::scene::SceneBuilder& b, Rgb col, F&& build) {
        const auto c = termchart::Color::rgb(col.r, col.g, col.b);
        b.fill(c);
        build();
        b.fill(c);
    }

    std::vector<termchart::Point> pts_;
    std::string title_ = "Wind";
    double rmax_ = 0.0;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_POLAR_RENDERER_HPP
