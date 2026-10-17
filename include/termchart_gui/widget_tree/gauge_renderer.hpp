// widget_tree/gauge_renderer.hpp —— 仪表盘（GUI 原生渲染插头）
//
// 把核心库里"字符化"的 gauge（落 '●' 格子）换成像素级仪表盘：270° 彩色分段弧带
// （AQI 优/良/轻度/中度/重度）+ 指针 + 轴帽 + 中心数值/类别。直接吐 Scene 指令，
// 经 SceneRenderer 光栅成干净像素，复用 ChartWidget 的动画/缓存/暗色主题。
// 全部画在 base 图层（不透明），area/line 层留空；不做悬停（hit_index 恒 -1）。
#ifndef TERMCHART_GUI_WIDGET_TREE_GAUGE_RENDERER_HPP
#define TERMCHART_GUI_WIDGET_TREE_GAUGE_RENDERER_HPP

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart_gui/backend_tk/tk_canvas.hpp"
#include "termchart_gui/widget_tree/i_chart_renderer.hpp"

namespace termchart_gui {

class GaugeRenderer final : public IChartRenderer {
public:
    void set_value(double v) { value_ = v; }
    void set_max(double m) { vmax_ = m > 0.0 ? m : 1.0; }
    void set_title(std::string s) { title_ = std::move(s); }
    // 各分段上界 + 颜色（按上界升序）。默认中国 AQI 分段。
    void set_zones(std::vector<std::pair<double, Rgb>> z) { zones_ = std::move(z); }

    [[nodiscard]] bool empty() const override { return false; }

    [[nodiscard]] ChartGeom compute_geom(int W, int H) const override {
        ChartGeom g;
        g.n = 1;
        g.pl = 20.0;
        g.pt = 20.0;
        g.pr = static_cast<double>(W) - 14.0;
        g.pb = static_cast<double>(H) - 14.0;
        return g;
    }

    void build_base_scene(termchart::scene::SceneBuilder& b, const ChartGeom& g,
                          double /*glow*/, bool /*dragging*/) const override {
        const double cx = (g.pl + g.pr) / 2.0;
        const double cy = (g.pt + g.pb) / 2.0;
        const double R = std::min(g.pr - g.pl, g.pb - g.pt) / 2.0 * 0.82;
        const double band = std::max(8.0, R * 0.14);
        const double a0 = 135.0;   // 起始（左下，值最小）
        const double sweep = 270.0;  // 顺时针扫 270°，底部开口

        arc_band(b, cx, cy, R, band, a0, a0 + sweep, Rgb{54, 58, 66});  // 背景轨道

        double prev = 0.0;  // 分段色带
        for (const auto& z : zones_) {
            const double f0 = clamp01(prev / vmax_);
            const double f1 = clamp01(z.first / vmax_);
            if (f1 > f0) arc_band(b, cx, cy, R, band, a0 + f0 * sweep, a0 + f1 * sweep, z.second);
            prev = z.first;
        }

        const double f = clamp01(value_ / vmax_);  // 指针
        const double ang = (a0 + f * sweep) * kPI / 180.0;
        const double rr = R * 0.74;
        stroke_path(b, Rgb{238, 240, 246}, 3.0, [&] {
            b.move_to(cx, cy);
            b.line_to(cx + rr * std::cos(ang), cy + rr * std::sin(ang));
        });
        fill_path(b, Rgb{238, 240, 246},  // 轴帽
                  [&] { b.arc(termchart::Point{cx, cy}, band * 0.42, 0.0, 2.0 * kPI); });

        // 文案（GUI 字体仅 ASCII，故用 ASCII）。
        Style white;
        white.set_fg(termchart::Color::rgb(235, 238, 244));
        Style gray;
        gray.set_fg(termchart::Color::rgb(150, 152, 162));
        b.text(termchart::Point{cx - static_cast<double>(title_.size()) * 4.5, g.pt - 2.0}, title_,
               white);
        const std::string val = std::to_string(static_cast<int>(std::lround(value_)));
        b.text(termchart::Point{cx - static_cast<double>(val.size()) * 4.5, cy + 2.0}, val, white);
        const std::string cat = aqi_category(value_);
        b.text(termchart::Point{cx - static_cast<double>(cat.size()) * 4.5, cy + 14.0}, cat, gray);
        b.text(termchart::Point{cx - 9.0, cy + 26.0}, "AQI", gray);
        b.text(termchart::Point{g.pl, g.pb - 8.0}, "0", gray);
        const std::string mx = std::to_string(static_cast<int>(vmax_));
        b.text(termchart::Point{g.pr - static_cast<double>(mx.size()) * 9.0, g.pb - 8.0}, mx, gray);
    }

    void build_area_scene(termchart::scene::SceneBuilder&, const ChartGeom&, double) const override {}
    void build_line_scene(termchart::scene::SceneBuilder&, const ChartGeom&, double) const override {}
    [[nodiscard]] int hit_index(const ChartGeom&, double, double) const override { return -1; }
    void draw_hover(TkCanvas&, const Rect&, const ChartGeom&, int, double) const override {}
    void draw_tooltip(TkCanvas&, const Rect&, const ChartGeom&, int) const override {}

private:
    static constexpr double kPI = 3.141592653589793;

    static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

    template <typename F>
    static void stroke_path(termchart::scene::SceneBuilder& b, Rgb col, double w, F&& build) {
        const auto c = termchart::Color::rgb(col.r, col.g, col.b);
        b.stroke(c, w);
        build();
        b.stroke(c, w);  // 收口（用旧状态着色，故末尾再发一次）
    }
    template <typename F>
    static void fill_path(termchart::scene::SceneBuilder& b, Rgb col, F&& build) {
        const auto c = termchart::Color::rgb(col.r, col.g, col.b);
        b.fill(c);
        build();
        b.fill(c);
    }
    static void arc_band(termchart::scene::SceneBuilder& b, double cx, double cy, double r, double w,
                         double a0deg, double a1deg, Rgb col) {
        const int N = 56;
        const double a0 = a0deg * kPI / 180.0;
        const double a1 = a1deg * kPI / 180.0;
        const auto c = termchart::Color::rgb(col.r, col.g, col.b);
        b.stroke(c, w);
        b.move_to(cx + r * std::cos(a0), cy + r * std::sin(a0));
        for (int i = 1; i <= N; ++i) {
            const double t = a0 + (a1 - a0) * static_cast<double>(i) / static_cast<double>(N);
            b.line_to(cx + r * std::cos(t), cy + r * std::sin(t));
        }
        b.stroke(c, w);
    }

    static std::string aqi_category(double v) {
        if (v <= 50.0) return "Good";
        if (v <= 100.0) return "Mod";
        if (v <= 150.0) return "Sens";
        if (v <= 200.0) return "Bad";
        if (v <= 300.0) return "V.Bad";
        return "Haz";
    }

    double value_ = 0.0;
    double vmax_ = 300.0;
    std::string title_ = "AQI";
    std::vector<std::pair<double, Rgb>> zones_ = {
        {50, {80, 200, 120}}, {100, {240, 200, 70}}, {150, {240, 150, 60}},
        {200, {230, 90, 80}},  {300, {170, 110, 200}}};
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_GAUGE_RENDERER_HPP
