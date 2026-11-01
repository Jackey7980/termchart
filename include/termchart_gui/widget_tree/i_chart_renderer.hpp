// widget_tree/i_chart_renderer.hpp —— 图表渲染"插头"接口 + 共享几何/平滑辅助
//
// "骨肉分离"的契约层：ChartWidget（万能插座）只负责动画/缓存/降分辨率/淡入/悬停
// 十字轴，把"具体画什么"交给 IChartRenderer 子类（ComboRenderer / HsiKLineRenderer）。
// 二者通过 ChartGeom（纯数据几何）与 ChartWidget 的动画状态解耦。
#ifndef TERMCHART_GUI_WIDGET_TREE_I_CHART_RENDERER_HPP
#define TERMCHART_GUI_WIDGET_TREE_I_CHART_RENDERER_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/scene/scene.hpp"
#include "termchart/util/math.hpp"
#include "termchart_gui/backend_tk/tk_canvas.hpp"
#include "termchart_gui/fwd.hpp"

namespace termchart_gui {

// 共享几何：控件局部像素坐标系下的绘图区 + 数据派生布局。
// 折线/面积层（combo）用 ly/m；K 线（HSI）不用（留空）。
struct ChartGeom {
    double pl = 0, pt = 0, pr = 0, pb = 0;  // plot 矩形（控件局部像素）
    double ytop = 0;                         // y 域顶部（随数据）
    double step = 0;                         // nice 步长（随数据）
    int intervals = 0;
    std::vector<double> xs;  // 各类别/蜡烛中心 x（控件局部）
    std::vector<double> ly;  // 各折线点 y（控件局部；K 线留空）
    std::vector<double> m;   // 各点单调三次切线（Fritsch-Carlson；K 线留空）
    int n = 0;
};

// —— 折线平滑所需辅助（combo 用；HSI 留空字段，这些函数天然 no-op）——
inline double nice_step(double rough) {
    if (!(rough > 0.0)) return 1.0;
    const double mag = std::pow(10.0, std::floor(std::log10(rough)));
    const double n = rough / mag;
    double m;
    if (n <= 1.0) m = 1.0;
    else if (n <= 2.0) m = 2.0;
    else if (n <= 2.5) m = 2.5;
    else if (n <= 5.0) m = 5.0;
    else m = 10.0;
    return m * mag;
}

inline std::string fmt_int(double v) {
    return std::to_string(static_cast<int>(std::lround(v)));
}

inline void recompute_tangents(ChartGeom& g) {
    // 防御：xs/ly 长度可能 != n（非平滑插头 gauge/polar 把 xs/ly 留空），按 min 收敛，
    // 绝不越界下标。实时数据过渡（tick 动画收敛）时若几何不齐，这里兜底不 abort。
    const int n = std::min({g.n, static_cast<int>(g.xs.size()),
                            static_cast<int>(g.ly.size())});
    g.m.assign(static_cast<std::size_t>(n < 0 ? 0 : n), 0.0);
    if (n < 2) return;
    std::vector<double> d(static_cast<std::size_t>(n - 1), 0.0);
    for (int k = 0; k + 1 < n; ++k) {
        const double dx = g.xs[static_cast<std::size_t>(k + 1)] - g.xs[static_cast<std::size_t>(k)];
        d[static_cast<std::size_t>(k)] =
            (dx != 0.0)
                ? (g.ly[static_cast<std::size_t>(k + 1)] - g.ly[static_cast<std::size_t>(k)]) / dx
                : 0.0;
    }
    g.m[0] = d[0];
    g.m[static_cast<std::size_t>(n - 1)] = d[static_cast<std::size_t>(n - 2)];
    for (int k = 1; k + 1 < n; ++k) {
        g.m[static_cast<std::size_t>(k)] =
            0.5 * (d[static_cast<std::size_t>(k - 1)] + d[static_cast<std::size_t>(k)]);
    }
    for (int k = 0; k + 1 < n; ++k) {
        if (d[static_cast<std::size_t>(k)] == 0.0) {
            g.m[static_cast<std::size_t>(k)] = 0.0;
            g.m[static_cast<std::size_t>(k + 1)] = 0.0;
            continue;
        }
        const double a = g.m[static_cast<std::size_t>(k)] / d[static_cast<std::size_t>(k)];
        const double bb = g.m[static_cast<std::size_t>(k + 1)] / d[static_cast<std::size_t>(k)];
        const double s = a * a + bb * bb;
        if (s > 9.0) {
            const double tau = 3.0 / std::sqrt(s);
            g.m[static_cast<std::size_t>(k)] = tau * a * d[static_cast<std::size_t>(k)];
            g.m[static_cast<std::size_t>(k + 1)] = tau * bb * d[static_cast<std::size_t>(k)];
        }
    }
}

inline bool geom_equal(const ChartGeom& a, const ChartGeom& b) {
    if (a.n != b.n || a.xs.size() != b.xs.size() || a.ly.size() != b.ly.size()) return false;
    if (std::fabs(a.pl - b.pl) >= 0.5 || std::fabs(a.pt - b.pt) >= 0.5 ||
        std::fabs(a.pr - b.pr) >= 0.5 || std::fabs(a.pb - b.pb) >= 0.5 ||
        std::fabs(a.ytop - b.ytop) >= 0.5) {
        return false;
    }
    for (std::size_t i = 0; i < a.ly.size(); ++i) {
        if (std::fabs(a.ly[i] - b.ly[i]) >= 0.5) return false;
    }
    for (std::size_t i = 0; i < a.xs.size(); ++i) {
        if (std::fabs(a.xs[i] - b.xs[i]) >= 0.5) return false;
    }
    return true;
}

// 过渡显示几何：plot 矩形立即吸附到目标（撑满），只对内容（xs、ly、ytop）逐元素插值。
inline ChartGeom lerp_geom(const ChartGeom& a, const ChartGeom& b, double e) {
    // 几何不变式：xs/ly 长度必须 == n。gauge/polar 这类"非平滑"插头只设 n=1 而把 xs/ly 留空，
    // 若直接插值会在 recompute_tangents / smooth_y_at 里越界 abort。不变式不齐就直接吸附到目标。
    const bool inv_a = (a.xs.size() == static_cast<std::size_t>(a.n)) &&
                        (a.ly.size() == static_cast<std::size_t>(a.n));
    const bool inv_b = (b.xs.size() == static_cast<std::size_t>(b.n)) &&
                        (b.ly.size() == static_cast<std::size_t>(b.n));
    if (a.n != b.n || !inv_a || !inv_b) return b;
    ChartGeom g;
    g.n = a.n;
    g.pl = b.pl;  // 框架吸附到目标（撑满），不插值
    g.pt = b.pt;
    g.pr = b.pr;
    g.pb = b.pb;
    g.ytop = termchart::util::lerp(a.ytop, b.ytop, e);
    g.step = b.step;
    g.intervals = b.intervals;
    g.xs.resize(static_cast<std::size_t>(a.n));  // 按 n 定长，保证 xs/ly 长度 == n（供 smooth_y_at 安全下标）
    g.ly.resize(static_cast<std::size_t>(a.n));
    for (int i = 0; i < a.n; ++i) {
        g.xs[static_cast<std::size_t>(i)] =
            termchart::util::lerp(a.xs[static_cast<std::size_t>(i)],
                                  b.xs[static_cast<std::size_t>(i)], e);
    }
    for (int i = 0; i < a.n; ++i) {
        g.ly[static_cast<std::size_t>(i)] =
            termchart::util::lerp(a.ly[static_cast<std::size_t>(i)],
                                  b.ly[static_cast<std::size_t>(i)], e);
    }
    recompute_tangents(g);
    return g;
}

// 平滑曲线在 x 处的 y（单调三次）。
inline double smooth_y_at(const ChartGeom& g, double x) {
    const int N = g.n;
    if (N <= 0) return g.pb;
    if (N == 1 || x <= g.xs[0]) return g.ly[0];
    if (x >= g.xs[static_cast<std::size_t>(N - 1)]) {
        return g.ly[static_cast<std::size_t>(N - 1)];
    }
    int i = 0;
    while (i + 1 < N && g.xs[static_cast<std::size_t>(i + 1)] < x) ++i;
    const double h = g.xs[static_cast<std::size_t>(i + 1)] - g.xs[static_cast<std::size_t>(i)];
    if (h <= 0.0) return g.ly[static_cast<std::size_t>(i)];
    double t = (x - g.xs[static_cast<std::size_t>(i)]) / h;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
    const double h10 = t3 - 2.0 * t2 + t;
    const double h01 = -2.0 * t3 + 3.0 * t2;
    const double h11 = t3 - t2;
    return h00 * g.ly[static_cast<std::size_t>(i)] + h10 * h * g.m[static_cast<std::size_t>(i)] +
           h01 * g.ly[static_cast<std::size_t>(i + 1)] +
           h11 * h * g.m[static_cast<std::size_t>(i + 1)];
}

// 插头（Renderer 接口）："肉"——具体的图表绘制逻辑。
// ChartWidget 持有一个实例，负责动画/缓存/降分辨率/淡入/悬停十字轴；本接口只管
// "给定几何 + 数据，画进 Scene / 画悬停"。
class IChartRenderer {
public:
    virtual ~IChartRenderer() = default;

    // 底层背景/前景色（ChartWidget 光栅/合成用它）：combo 深色、HSI 浅色。
    [[nodiscard]] virtual Rgb background() const { return Rgb{24, 26, 32}; }
    [[nodiscard]] virtual Rgb foreground() const { return Rgb{229, 229, 229}; }

    // 是否尚无数据（ChartWidget 防呆：空时不画）。
    [[nodiscard]] virtual bool empty() const = 0;

    // 由数据 + 控件尺寸算目标几何（纯函数）。
    [[nodiscard]] virtual ChartGeom compute_geom(int W, int H) const = 0;

    // 三图层 Scene 构建。provisional 快路径只需 base+line；静止精美帧三图层合成。
    // dragging 用于冻结 x 轴标签抽稀步长（仅 combo 语义相关）。
    virtual void build_base_scene(termchart::scene::SceneBuilder& b, const ChartGeom& g,
                                  double glow, bool dragging) const = 0;
    virtual void build_area_scene(termchart::scene::SceneBuilder& b, const ChartGeom& g,
                                  double glow) const = 0;
    virtual void build_line_scene(termchart::scene::SceneBuilder& b, const ChartGeom& g,
                                  double glow) const = 0;

    // 悬停命中（局部 x,y → 下标或 -1）与已提交的 x 轴标签抽稀步长。
    [[nodiscard]] virtual int hit_index(const ChartGeom& g, double lx, double ly) const = 0;
    [[nodiscard]] virtual int label_step() const { return 1; }

    // 悬停该下标时的"强调"绘制（combo=柱高亮描边；HSI=蜡烛高亮）与信息框。
    virtual void draw_hover(TkCanvas& c, const Rect& g, const ChartGeom& geom, int hovered,
                            double glow) const = 0;
    virtual void draw_tooltip(TkCanvas& c, const Rect& g, const ChartGeom& geom,
                              int hovered) const = 0;

    // ChartWidget::set_data 默认转发到这里（仅 combo 有语义；HSI 无操作）。
    virtual void set_combo_data(std::vector<std::string> labels, std::vector<double> bars,
                                std::vector<double> line) {
        (void)labels;
        (void)bars;
        (void)line;
    }
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_I_CHART_RENDERER_HPP
