// widget_tree/combo_renderer.hpp —— 多系列组合图渲染（深色主题，可配置）
//
// "骨肉分离"后的"肉"之一：柱 + 折线 + 面积 的任意组合，作为可插拔 IChartRenderer。
// 默认参数完全复刻原 ChartWidget 的组合图外观（柱蓝 / 面积青 / 折线青 + 标题），
// 故既有 956 测试零改动；天气等场景用 set_series / set_line_colors / set_area /
// set_marker / set_show_bars / set_title 重新配置即可。ChartWidget（插座）复用其
// 动画/缓存/降分辨率/淡入/悬停十字轴。
#ifndef TERMCHART_GUI_WIDGET_TREE_COMBO_RENDERER_HPP
#define TERMCHART_GUI_WIDGET_TREE_COMBO_RENDERER_HPP

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "termchart/core/style.hpp"
#include "termchart_gui/backend_tk/tk_canvas.hpp"
#include "termchart_gui/fwd.hpp"
#include "termchart_gui/theme.hpp"
#include "termchart_gui/widget_tree/i_chart_renderer.hpp"

namespace termchart_gui {

class ComboRenderer : public IChartRenderer {
public:
    void set_combo_data(std::vector<std::string> labels, std::vector<double> bars,
                        std::vector<double> line) override {
        set_series(std::move(labels), std::move(bars), {std::move(line)});
    }

    [[nodiscard]] Rgb background() const override { return current_theme().background; }
    [[nodiscard]] Rgb foreground() const override { return current_theme().foreground; }

    // 多系列：类别标签 + 柱（可空）→ 折线系列（>=1 条）。
    // 防御性：把 bars_ 与每条折线补齐到标签长度（缺的填 0），否则标签比数据长时
    // compute_geom/build_line_scene/draw_* 里 lines_[k][i] 会越界 abort（weather_demo
    // 喂 Open-Meteo 实时数据时，缺失小时 null 会让某数组比 time 短）。
    void set_series(std::vector<std::string> labels, std::vector<double> bars,
                    std::vector<std::vector<double>> lines,
                    std::vector<std::string> names = {}) {
        labels_ = std::move(labels);
        bars_ = std::move(bars);
        lines_ = std::move(lines);
        const std::size_t N = labels_.size();
        if (bars_.size() < N) bars_.resize(N, 0.0);
        for (auto& ln : lines_) {
            if (ln.size() < N) ln.resize(N, 0.0);
        }
        if (names.empty()) {
            names_.resize(lines_.size());
            for (std::size_t k = 0; k < lines_.size(); ++k) {
                names_[k] = "L" + std::to_string(k);
            }
        } else {
            names_ = std::move(names);
        }
        if (line_colors_.size() != lines_.size()) {
            line_colors_.assign(lines_.size(), Rgb{80, 226, 247});
        }
    }

    // 风格配置（天气等场景调用）。
    void set_line_colors(std::vector<Rgb> cols) { line_colors_ = std::move(cols); }
    void set_bar_color(Rgb c) { bar_color_ = c; }
    void set_bar_name(const std::string& s) { bar_name_ = s; }
    // area_on < 0 关闭面积；否则在对应折线下填充面积（用 area_color）。
    void set_area(int area_on, Rgb area_color) {
        area_on_ = area_on;
        area_color_ = area_color;
    }
    // 在指定折线上标注其最大值（高亮点）。
    void set_marker(int line, Rgb color) {
        marker_line_ = line;
        marker_color_ = color;
    }
    void set_show_bars(bool on) { show_bars_ = on; }
    void set_title(std::string s) { title_ = std::move(s); }
    // 双 Y 轴：开启后，right_idx 中的折线系列走独立的右轴量纲（如温度），
    // 网格只按左轴画（保持背景干净），右轴右侧加刻度字，图例/tooltip 标 "(R)"。
    // 默认关闭 → 单轴，完全复刻原外观（既有用例零改动）。
    void set_right_axis(bool on, std::vector<int> idx = {}) {
        right_axis_ = on;
        right_idx_ = std::move(idx);
    }

    [[nodiscard]] bool empty() const override { return labels_.empty(); }

    // 悬停点的 y 位置（首条折线或柱顶）+ 读数标签，供水平十字线与实时数值。
    [[nodiscard]] double hover_y(const ChartGeom& g, int hovered) const override {
        if (hovered < 0 || hovered >= g.n) return std::nan("");
        const double d = (g.ytop - ymin_) > 0.0 ? (g.ytop - ymin_) : 1.0;
        if (!lines_.empty()) {
            const auto& ln = lines_.front();
            if (hovered < static_cast<int>(ln.size()))
                return g.pb - (g.pb - g.pt) * (ln[static_cast<std::size_t>(hovered)] - ymin_) / d;
        }
        if (!bars_.empty() && hovered < static_cast<int>(bars_.size()))
            return g.pb - (g.pb - g.pt) * (bars_[static_cast<std::size_t>(hovered)] - ymin_) / d;
        return std::nan("");
    }
    [[nodiscard]] std::string hover_label(const ChartGeom& g, int hovered) const override {
        (void)g;
        if (hovered < 0 || hovered >= g.n) return std::string{};
        if (!lines_.empty()) {
            const auto& ln = lines_.front();
            if (hovered < static_cast<int>(ln.size())) return fmt_int(ln[static_cast<std::size_t>(hovered)]);
        }
        if (!bars_.empty() && hovered < static_cast<int>(bars_.size()))
            return fmt_int(bars_[static_cast<std::size_t>(hovered)]);
        return std::string{};
    }

    // 由当前数据 + 尺寸算出目标几何（纯函数）。受 view_（Pan/Zoom）约束：只算可见
    // 下标区间 [lo,hi) 的量纲与 x 映射；ymin/ymax 覆盖左轴量纲。默认视图几何不变。
    [[nodiscard]] ChartGeom compute_geom(int W, int H) const override {
        ChartGeom g;
        const int N = static_cast<int>(labels_.size());
        g.n = N;
        g.pl = 46.0;
        g.pt = 46.0;
        g.pr = static_cast<double>(W) - (right_axis_ ? 42.0 : 14.0);  // 右轴占用右边距
        g.pb = static_cast<double>(H) - 26.0;

        const ViewWindow v = view_;
        int hi = (v.hi < 0 || v.hi > N) ? N : v.hi;
        int lo = v.lo;
        if (lo < 0) lo = 0;
        if (lo > hi) lo = hi;
        const int M = hi - lo;
        if (M <= 0) {
            g.xs.resize(static_cast<std::size_t>(N));
            ymin_ = 0.0;
            return g;
        }

        // 左轴量纲：柱 + 非右轴折线（仅可见区间）；右轴量纲：右轴折线。
        double lmax = 0.0;
        for (int i = lo; i < hi; ++i) {
            const std::size_t k = static_cast<std::size_t>(i);
            if (k < bars_.size()) lmax = std::max(lmax, bars_[k]);
            for (int kk = 0; kk < static_cast<int>(lines_.size()); ++kk) {
                if (on_right_(kk)) continue;
                const auto& ln = lines_[static_cast<std::size_t>(kk)];
                if (k < ln.size()) lmax = std::max(lmax, ln[k]);
            }
        }
        const bool y_override = !view_is_nan(v.ymin) || !view_is_nan(v.ymax);
        if (y_override) {
            const double y0 = view_is_nan(v.ymin) ? 0.0 : v.ymin;
            const double y1 =
                view_is_nan(v.ymax) ? (lmax > 0.0 ? lmax : 1.0) : v.ymax;
            const double span = (y1 - y0) > 0.0 ? (y1 - y0) : 1.0;
            g.step = nice_step(span / 4.0);
            g.intervals = std::max(1, static_cast<int>(std::ceil(span / g.step)));
            g.ytop = g.step * static_cast<double>(g.intervals);
            ymin_ = y0;
        } else {
            if (lmax <= 0.0) lmax = 1.0;
            g.step = nice_step(lmax / 4.0);
            g.intervals = std::max(1, static_cast<int>(std::ceil(lmax / g.step)));
            g.ytop = g.step * static_cast<double>(g.intervals);
            ymin_ = 0.0;
        }

        if (right_axis_) {
            double rmax = 0.0;
            for (int k = 0; k < static_cast<int>(lines_.size()); ++k) {
                if (!on_right_(k)) continue;
                for (double vv : lines_[static_cast<std::size_t>(k)]) rmax = std::max(rmax, vv);
            }
            if (rmax <= 0.0) rmax = 1.0;
            rstep_ = nice_step(rmax / 4.0);
            rintervals_ = std::max(1, static_cast<int>(std::ceil(rmax / rstep_)));
            rytop_ = rstep_ * static_cast<double>(rintervals_);
        }

        const int ai = (area_on_ >= 0 && area_on_ < static_cast<int>(lines_.size()))
                            ? area_on_
                            : (lines_.empty() ? 0 : 0);
        g.xs.resize(static_cast<std::size_t>(N));
        if (area_on_ >= 0 && area_on_ < static_cast<int>(lines_.size())) {
            g.ly.resize(static_cast<std::size_t>(N));
        }
        const double yden = (g.ytop - ymin_) > 0.0 ? (g.ytop - ymin_) : 1.0;
        for (int i = lo; i < hi; ++i) {
            g.xs[static_cast<std::size_t>(i)] =
                g.pl + (g.pr - g.pl) * (static_cast<double>(i - lo) + 0.5) /
                           static_cast<double>(M);
            if (area_on_ >= 0 && area_on_ < static_cast<int>(lines_.size())) {
                g.ly[static_cast<std::size_t>(i)] =
                    g.pb - (g.pb - g.pt) *
                               (lines_[static_cast<std::size_t>(ai)]
                                       [static_cast<std::size_t>(i)] -
                                ymin_) /
                               yden;
            }
        }
        for (int i = 0; i < lo; ++i) g.xs[static_cast<std::size_t>(i)] = g.pl - 1.0;
        for (int i = hi; i < N; ++i) g.xs[static_cast<std::size_t>(i)] = g.pr + 1.0;
        if (area_on_ >= 0 && area_on_ < static_cast<int>(lines_.size())) {
            recompute_tangents(g);
        }
        return g;
    }

    // 底层：网格 + 外框 + 柱 + x 标签 + 图例 + 标题（不透明背景）。
    void build_base_scene(termchart::scene::SceneBuilder& b, const ChartGeom& g, double glow,
                          bool dragging) const override {
        const int N = g.n;
        const auto to_y = [&](double v) {
            const double d = (g.ytop - ymin_) > 0.0 ? (g.ytop - ymin_) : 1.0;
            return g.pb - (g.pb - g.pt) * (v - ymin_) / d;
        };
        const int vlo = std::max(0, view_.lo);
        const int vhi = (view_.hi < 0 || view_.hi > N) ? N : view_.hi;
        const auto cx = [&](int i) { return g.xs[static_cast<std::size_t>(i)]; };
        Style gray;
        gray.set_fg(termchart::Color::rgb(current_theme().text_dim));
        Style title;
        title.set_fg(termchart::Color::rgb(current_theme().foreground));

        b.stroke(termchart::Color::rgb(current_theme().grid), 1.0);
        for (int k = 0; k <= g.intervals; ++k) {
            const double v = g.step * static_cast<double>(k);
            const double y = to_y(v);
            b.move_to(g.pl, y);
            b.line_to(g.pr, y);
            const std::string lab = fmt_int(v);
            b.text(Point{g.pl - 8.0 - static_cast<double>(lab.size()) * 9.0, y - 7.0}, lab, gray);
        }
        b.stroke(termchart::Color::rgb(current_theme().axis), 1.0);
        b.rect(Rect{g.pl, g.pt, g.pr - g.pl, g.pb - g.pt});

        // 右轴刻度字（仅双轴时；与右轴折线同色提示）。网格仍只按左轴画，保持干净。
        if (right_axis_) {
            const auto to_yr = [&](double v) { return g.pb - (g.pb - g.pt) * v / rytop_; };
            const Rgb rc = !right_idx_.empty() && right_idx_[0] < static_cast<int>(line_colors_.size())
                               ? line_colors_[static_cast<std::size_t>(right_idx_[0])]
                               : Rgb{150, 152, 162};
            Style rsty;
            rsty.set_fg(termchart::Color::rgb(rc.r, rc.g, rc.b));
            for (int k = 0; k <= rintervals_; ++k) {
                const double v = rstep_ * static_cast<double>(k);
                const double y = to_yr(v);
                const std::string lab = fmt_int(v);
                b.text(Point{g.pr + 6.0, y - 7.0}, lab, rsty);
            }
        }

        if (show_bars_ && !bars_.empty()) {
            const double M = static_cast<double>(vhi - vlo > 0 ? vhi - vlo : 1);
            const double bw = (g.pr - g.pl) / M * 0.55;
            b.fill(glow_lighten(bar_color_, glow));
            for (int i = vlo; i < vhi; ++i) {
                const double y = to_y(bars_[static_cast<std::size_t>(i)]);
                b.rect(Rect{cx(i) - bw / 2.0, y, bw, g.pb - y});
            }
        }
        // x 类别标签抽稀步长：松手/静态时按可用宽度现算并**提交**；拖动中**冻结**。
        int lstep;
        if (dragging) {
            lstep = label_step_;
        } else {
            const double slot =
                (g.pr - g.pl) / static_cast<double>(vhi - vlo > 0 ? vhi - vlo : 1);
            lstep = std::max(1, static_cast<int>(std::ceil(44.0 / (slot > 1.0 ? slot : 1.0))));
            label_step_ = lstep;
        }
        for (int i = vlo; i < vhi; ++i) {
            if (i % lstep != 0 && i != vhi - 1) {
                continue;
            }
            const double w =
                static_cast<double>(labels_[static_cast<std::size_t>(i)].size()) * 9.0;
            b.text(Point{cx(i) - w / 2.0, g.pb + 6.0}, labels_[static_cast<std::size_t>(i)],
                   gray);
        }
        // 图例（顺序布局：柱 + 各折线；默认"bar"/"line"时与原组合图坐标一致）。
        double lx = g.pl;
        if (show_bars_ && !bars_.empty()) {
            b.fill(glow_lighten(bar_color_, glow));
            b.rect(Rect{lx, g.pt - 32.0, 12.0, 10.0});
            b.text(Point{lx + 18.0, g.pt - 31.0}, bar_name_, gray);
            lx += 18.0 + static_cast<double>(bar_name_.size()) * 9.0 + 27.0;
        }
        for (std::size_t k = 0; k < lines_.size(); ++k) {
            const Rgb c = (k < line_colors_.size()) ? line_colors_[k] : Rgb{80, 226, 247};
            b.fill(glow_lighten(c, glow));
            b.rect(Rect{lx, g.pt - 32.0, 12.0, 10.0});
            std::string nm = (k < names_.size()) ? names_[k] : ("L" + std::to_string(k));
            if (on_right_(static_cast<int>(k))) nm += " (R)";  // 右轴系列标注
            b.text(Point{lx + 18.0, g.pt - 31.0}, nm, gray);
            lx += 18.0 + static_cast<double>(nm.size()) * 9.0 + 27.0;
        }
        b.text(Point{g.pl, 8.0}, title_, title);
    }

    // 面积层：在 area_on 折线下填充（透明背景，供半透明合成）。顶边走同一条平滑曲线。
    void build_area_scene(termchart::scene::SceneBuilder& b, const ChartGeom& g,
                          double /*glow*/) const override {
        if (area_on_ < 0 || area_on_ >= static_cast<int>(lines_.size())) {
            return;  // 未启用面积
        }
        b.fill(termchart::Color::rgb(area_color_.r, area_color_.g, area_color_.b));
        const int x0 = static_cast<int>(g.pl);
        const int x1 = static_cast<int>(g.pr);
        for (int x = x0; x <= x1; ++x) {
            const double yy = smooth_y_at(g, static_cast<double>(x) + 0.5) - 0.5;
            if (yy < g.pb) {
                b.rect(Rect{static_cast<double>(x), yy, 1.0, g.pb - yy});
            }
        }
    }

    // 折线层：area_on 系列走平滑曲线（与面积顶边几何一致）；其余系列走折线。
    // 逐点小圆点；marker_line 系列额外画最大值高亮。右轴系列（on_right_）按右轴量纲映射。
    // 所有绘制限制在可见区间 [vlo,vhi)（Pan/Zoom）。
    void build_line_scene(termchart::scene::SceneBuilder& b, const ChartGeom& g,
                          double glow) const override {
        const int N = g.n;
        if (N <= 0 || lines_.empty()) return;
        const int vlo = std::max(0, view_.lo);
        const int vhi = (view_.hi < 0 || view_.hi > N) ? N : view_.hi;
        if (vhi <= vlo) return;
        const auto to_y = [&](double v) {
            const double d = (g.ytop - ymin_) > 0.0 ? (g.ytop - ymin_) : 1.0;
            return g.pb - (g.pb - g.pt) * (v - ymin_) / d;
        };
        const auto to_yr = [&](double v) { return g.pb - (g.pb - g.pt) * v / rytop_; };
        const int S = 16;
        for (std::size_t k = 0; k < lines_.size(); ++k) {
            const bool right = on_right_(static_cast<int>(k));
            const auto map_y = [&](double v) { return right ? to_yr(v) : to_y(v); };
            const Rgb c = (k < line_colors_.size()) ? line_colors_[k] : Rgb{80, 226, 247};
            b.stroke(glow_lighten(c, glow), 3.0);
            const bool smooth = (static_cast<int>(k) == area_on_);  // 面积系列恒走左轴
            if (smooth) {
                for (int i = vlo; i + 1 < vhi; ++i) {
                    const double xa = g.xs[static_cast<std::size_t>(i)];
                    const double xb = g.xs[static_cast<std::size_t>(i + 1)];
                    if (i == vlo) b.move_to(xa, smooth_y_at(g, xa));
                    for (int s = 1; s <= S; ++s) {
                        const double t = static_cast<double>(s) / static_cast<double>(S);
                        const double x = xa + (xb - xa) * t;
                        b.line_to(x, smooth_y_at(g, x));
                    }
                }
            } else {
                b.move_to(g.xs[static_cast<std::size_t>(vlo)],
                          map_y(lines_[static_cast<std::size_t>(k)]
                                   [static_cast<std::size_t>(vlo)]));
                for (int i = vlo + 1; i < vhi; ++i) {
                    b.line_to(g.xs[static_cast<std::size_t>(i)],
                              map_y(lines_[static_cast<std::size_t>(k)]
                                       [static_cast<std::size_t>(i)]));
                }
            }
            // 逐点小圆点
            b.fill(glow_lighten(c, glow));
            for (int i = vlo; i < vhi; ++i) {
                const double yy = smooth ? smooth_y_at(g, g.xs[static_cast<std::size_t>(i)])
                                         : map_y(lines_[static_cast<std::size_t>(k)]
                                                    [static_cast<std::size_t>(i)]);
                b.arc(Point{g.xs[static_cast<std::size_t>(i)], yy}, 3.0, 0.0, 6.283185307);
            }
        }
        // 最大值高亮（如 24h 最高温圆点，恒走左轴，限可见区间）
        if (marker_line_ >= 0 && marker_line_ < static_cast<int>(lines_.size()) &&
            vhi > vlo) {
            const auto& ln = lines_[static_cast<std::size_t>(marker_line_)];
            int mi = vlo;
            for (int i = vlo + 1; i < vhi; ++i) {
                if (ln[static_cast<std::size_t>(i)] > ln[static_cast<std::size_t>(mi)])
                    mi = i;
            }
            const double ym = to_y(ln[static_cast<std::size_t>(mi)]);
            b.stroke(termchart::Color::rgb(marker_color_.r, marker_color_.g, marker_color_.b),
                     2.0);
            b.arc(Point{g.xs[static_cast<std::size_t>(mi)], ym}, 6.0, 0.0, 6.283185307);
            b.fill(termchart::Color::rgb(marker_color_.r, marker_color_.g, marker_color_.b));
            b.arc(Point{g.xs[static_cast<std::size_t>(mi)], ym}, 3.5, 0.0, 6.283185307);
        }
    }

    [[nodiscard]] int hit_index(const ChartGeom& g, double lx, double /*ly*/) const override {
        const int N = g.n;
        if (N <= 0 || lx < g.pl || lx > g.pr) return -1;
        const int vlo = std::max(0, view_.lo);
        const int vhi = (view_.hi < 0 || view_.hi > N) ? N : view_.hi;
        int best = -1;
        double bd = 1e18;
        for (int i = vlo; i < vhi; ++i) {
            const double d = std::fabs(g.xs[static_cast<std::size_t>(i)] - lx);
            if (d < bd) {
                bd = d;
                best = i;
            }
        }
        return best;
    }

    [[nodiscard]] int label_step() const override { return label_step_; }

    // 悬停强调：柱描边 + 各折线悬停点小高亮圆。
    void draw_hover(TkCanvas& c, const Rect& g, const ChartGeom& geom, int hovered,
                   double /*glow*/) const override {
        const std::size_t h = static_cast<std::size_t>(hovered);
        const auto to_y = [&](double v) {
            return geom.pb - (geom.pb - geom.pt) * v / geom.ytop;
        };
        const auto to_yr = [&](double v) {
            return geom.pb - (geom.pb - geom.pt) * v / rytop_;
        };
        if (show_bars_ && !bars_.empty()) {
            const double hbw = (geom.pr - geom.pl) / static_cast<double>(geom.n) * 0.55;
            const double htop = to_y(bars_[h]);
            c.stroke_rect(Rect{g.x + geom.xs[h] - hbw / 2.0, g.y + htop, hbw, geom.pb - htop},
                          Rgb{150, 180, 255}, 1.0);
        }
        for (std::size_t k = 0; k < lines_.size(); ++k) {
            const bool right = on_right_(static_cast<int>(k));
            const auto map_y = [&](double v) { return right ? to_yr(v) : to_y(v); };
            const Rgb col = (k < line_colors_.size()) ? line_colors_[k] : Rgb{80, 226, 247};
            const double y = map_y(lines_[k][h]);
            c.fill_rect(Rect{g.x + geom.xs[h] - 3.0, g.y + y - 3.0, 6.0, 6.0}, col);
        }
    }

    void draw_tooltip(TkCanvas& c, const Rect& g, const ChartGeom& geom, int hovered) const override {
        const std::size_t h = static_cast<std::size_t>(hovered);
        const int rows = static_cast<int>(lines_.size()) + (show_bars_ && !bars_.empty() ? 1 : 0);
        const double bw = 132.0, bh = 8.0 + 16.0 * static_cast<double>(rows);
        double tx = g.x + geom.xs[h] + 12.0;
        double ty = g.y + geom.ly.empty()
                        ? (g.y + geom.pt + 4.0)
                        : (g.y + geom.ly[h] - bh - 8.0);
        if (tx + bw > g.x + g.w) tx = g.x + geom.xs[h] - bw - 12.0;
        if (tx < g.x) tx = g.x + 2.0;
        if (ty + bh > g.y + g.h) ty = g.y + g.h - bh - 2.0;
        if (ty < g.y) ty = g.y + 2.0;

        c.fill_rect(Rect{tx + 2.0, ty + 2.0, bw, bh}, current_theme().surface_alt);  // 投影
        c.fill_rect(Rect{tx, ty, bw, bh}, current_theme().tooltip_bg);              // 主体
        c.stroke_rect(Rect{tx, ty, bw, bh}, current_theme().tooltip_border, 1.0);   // 描边
        double yy = ty + 5.0;
        c.draw_text(tx + 9.0, yy, labels_[h], current_theme().tooltip_fg);          // 标题
        yy += 15.0;
        if (show_bars_ && !bars_.empty()) {
            c.fill_rect(Rect{tx + 9.0, yy + 2.0, 6.0, 6.0}, bar_color_);
            c.draw_text(tx + 19.0, yy, bar_name_ + " " + fmt_int(bars_[h]),
                       current_theme().tooltip_fg);
            yy += 15.0;
        }
        for (std::size_t k = 0; k < lines_.size(); ++k) {
            const Rgb col = (k < line_colors_.size()) ? line_colors_[k] : Rgb{80, 226, 247};
            std::string nm = (k < names_.size()) ? names_[k] : ("L" + std::to_string(k));
            if (on_right_(static_cast<int>(k))) nm += " (R)";
            c.fill_rect(Rect{tx + 9.0, yy + 2.0, 6.0, 6.0}, col);
            c.draw_text(tx + 19.0, yy, nm + " " + fmt_int(lines_[k][h]),
                       current_theme().tooltip_fg);
            yy += 15.0;
        }
    }

private:
    // 交互高光：基色向亮色轻插值（变化时闪一下、tick 衰减回基色），保持原青线外观。
    static termchart::Color glow_lighten(Rgb base, double g) {
        const double t = (g <= 0.0) ? 0.0 : (g > 1.0 ? 1.0 : g) * 0.4;
        const auto m = [&](int v) {
            const int x = static_cast<int>(std::lround(static_cast<double>(v) +
                                                      (255.0 - static_cast<double>(v)) * t));
            return static_cast<std::uint8_t>(x < 0 ? 0 : (x > 255 ? 255 : x));
        };
        return termchart::Color::rgb(m(base.r), m(base.g), m(base.b));
    }

    std::vector<std::string> labels_;
    std::vector<double> bars_;
    std::vector<std::vector<double>> lines_;
    std::vector<std::string> names_;
    std::vector<Rgb> line_colors_ = {{80, 226, 247}};
    Rgb bar_color_ = {59, 110, 246};
    Rgb area_color_ = {40, 170, 200};
    int area_on_ = 0;            // 在折线[area_on_]下填充面积；<0 关闭
    int marker_line_ = -1;       // 在折线[marker_line_]标最大值；<0 关闭
    Rgb marker_color_ = {255, 210, 120};
    bool show_bars_ = true;
    mutable double ymin_ = 0.0;    // 左轴量纲下界（compute_geom 回填；to_y 映射用；const 场景构建器可读写）
    std::string title_ = "Revenue vs Margin (GUI combo)";
    std::string bar_name_ = "bar";
    mutable int label_step_ = 1;  // 已提交的抽稀步长（拖动中沿用、松手重算）

    // 双 Y 轴：right_idx_ 中的折线系列走独立右轴量纲。
    bool right_axis_ = false;
    std::vector<int> right_idx_;
    mutable double rytop_ = 0.0;  // 右轴量纲顶端（由 compute_geom 算出，mutable 以便 const 场景构建器读取）
    mutable double rstep_ = 0.0;
    mutable int rintervals_ = 0;

    [[nodiscard]] bool on_right_(int k) const {
        if (!right_axis_ || k < 0 || k >= static_cast<int>(lines_.size())) return false;
        return std::find(right_idx_.begin(), right_idx_.end(), k) != right_idx_.end();
    }
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_COMBO_RENDERER_HPP
