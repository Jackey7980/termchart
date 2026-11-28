// widget_tree/chart_widget.hpp —— GUI 组合图表控件（"万能插座"）
//
// 复用边界内的图表控件：把"数据 → Scene（矢量 IR）→ SceneRenderer 光栅化 → blit 上屏"
// 封成一个 Widget。"画什么"由可插拔的 IChartRenderer 决定（ComboRenderer / HsiKLineRenderer），
// 本类只负责：动画（指数趋近 + smoothstep）、底图缓存、运动中降分辨率 + 高清淡入、
// 悬停十字轴。这样同一套插座能同时驱动 combo 与 K 线。
//
// 交互：
//   * 悬停：MouseMove 命中测试（委托 renderer.hit_index）→ 记录 hovered_，重绘时画十字轴
//     + renderer 的强调/信息框（近似 ECharts axisPointer/emphasis）。
//   * 缩放自适应 + 过渡动画：paint 检测到尺寸/数据变化时记下 target_，由 tick(dt) 推进
//     anim_t_（指数趋近，帧率解耦），每帧按插值几何重建 Scene 重绘——柱/折线平滑过渡。
//   * 动画驱动在调用方（示例事件循环）：Win32 离屏双缓冲 → 平滑无闪烁。
#ifndef TERMCHART_GUI_WIDGET_TREE_CHART_WIDGET_HPP
#define TERMCHART_GUI_WIDGET_TREE_CHART_WIDGET_HPP

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/gui/scene_renderer.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/scene/scene.hpp"
#include "termchart/util/math.hpp"
#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/tk_canvas.hpp"
#include "termchart_gui/fwd.hpp"
#include "termchart_gui/native_loop/event.hpp"
#include "termchart_gui/widget_tree/combo_renderer.hpp"
#include "termchart_gui/theme.hpp"
#include "termchart_gui/widget_tree/i_chart_renderer.hpp"
#include "termchart_gui/widget_tree/widget.hpp"

namespace termchart_gui {

class ChartWidget final : public Widget {
public:
    explicit ChartWidget(std::string id) : Widget(std::move(id)) {
        // 默认插头 = combo（保持向后兼容：未显式 set_renderer 时仍是柱+折线组合图）。
        renderer_ = std::make_shared<ComboRenderer>();
    }

    // 替换渲染插头（"万能插座"的核心）。
    void set_renderer(std::shared_ptr<IChartRenderer> r) {
        if (r) {
            renderer_ = std::move(r);
        }
    }

    // 设置三路对齐的数据（转发给当前插头；HSI 插头无 combo 语义时忽略）。
    void set_data(std::vector<std::string> labels, std::vector<double> bars,
                  std::vector<double> line) {
        renderer_->set_combo_data(std::move(labels), std::move(bars), std::move(line));
        base_.reset();
        hovered_ = -1;
        dirty_ = true;  // 下次 paint 走一次"数据 morph"过渡
    }

    [[nodiscard]] int hovered_index() const noexcept { return hovered_; }

    // 性能插桩开关：打开后每次重建底图向 stderr 打印 A(建Scene)/B(光栅)/C(合成) 耗时。
    static void set_profile(bool on) noexcept { profiling_ = on; }

    [[nodiscard]] bool animating() const noexcept {
        return (have_geom_ && !geom_equal(geom_, target_)) || fade_ < 1.0 || glow_ > 0.01;
    }

    // 请求下次 paint 直接把几何吸附到目标（resize 拖动中同步重渲染用）。
    void request_snap() noexcept { snap_pending_ = true; }

    // 拖动中标志：true=只调柱宽/间距（标签抽稀步长冻结）；false=松手/静态，按当前宽度重算。
    void set_dragging(bool on) {
        dragging_ = on;
        dirty_ = true;
        base_settled_ = false;  // 废掉底图缓存 → 强制下帧重建，按新 dragging 态重算抽稀/柱宽
    }

    // 数据已由外部（renderer）更新后请求重绘：绕过 set_data 的多系列/天气数据更新用。
    void invalidate() noexcept {
        dirty_ = true;
        base_settled_ = false;  // 废底图缓存 → 下帧按新数据重算几何并重绘（走过渡动画）
    }

    // 高 DPI：设置设备像素比（>1 时离屏画布按物理分辨率渲染，文本格同步放大，避免发糊）。
    void set_pixel_scale(double s) noexcept { pixel_scale_ = (s > 0.0) ? s : 1.0; }

    // 推进一帧（dt 秒）：指数趋近 target_（帧率解耦）。返回是否仍在动画中。
    bool tick(double dt_sec) {
        const double dt = dt_sec > 0.0 ? dt_sec : 0.0;
        bool moving = false;
        if (have_geom_ && !geom_equal(geom_, target_)) {
            const double f = 1.0 - std::exp(-kFollow * dt);
            geom_ = lerp_geom(geom_, target_, f);
            if (geom_equal(geom_, target_)) {
                geom_ = target_;  // 收敛后吸附，消除亚像素残差
            }
            moving = true;
        }
        if (fade_ < 1.0) {  // 高清帧交叉淡入推进
            fade_ = std::min(1.0, fade_ + dt / kFadeDur);
            moving = true;
        }
        if (glow_ > 0.0) {  // 交互高光指数衰减
            glow_ *= std::exp(-kGlowDecay * dt);
            if (glow_ < 0.01) {
                glow_ = 0.0;
            }
            moving = true;
        }
        return moving;
    }

    // 当前（可能处于动画中）绘图区右/下边界，供测试/调试。
    [[nodiscard]] double plot_right() const noexcept { return geom_.pr; }
    [[nodiscard]] double plot_bottom() const noexcept { return geom_.pb; }
    // 已提交的 x 轴标签抽稀步长（1=全显示；拖动中冻结、松手/静态按宽度重算）。
    [[nodiscard]] int label_step() const noexcept {
        return renderer_ ? renderer_->label_step() : 1;
    }

    void paint(TkCanvas& c) const override {
        const Rect g = global_bounds();
        const int W = static_cast<int>(g.w);
        const int H = static_cast<int>(g.h);
        if (W < 48 || H < 32 || !renderer_ || renderer_->empty()) {  // 防呆
            return;
        }
        const bool size_changed = have_geom_ && (base_w_ != W || base_h_ != H);
        if (!have_geom_ || dirty_ || size_changed) {
            const ChartGeom want = renderer_->compute_geom(W, H);
            if (!have_geom_ || snap_pending_) {
                geom_ = want;  // 首帧 / 拖动中同步重渲染：直接吸附到位，不等 tick
            }
            target_ = want;  // 其余只更新目标，由 tick 指数趋近
            if (have_geom_) {
                glow_ = std::min(1.0, glow_ + 0.5);  // 变化(非首帧) → 交互高光 boost
            }
            have_geom_ = true;
            dirty_ = false;
        }
        snap_pending_ = false;  // 用过即清

        const bool provisional = animating() || dragging_;  // 形变中/拖动中 → 低清快路径
        if (provisional || !base_ || base_w_ != W || base_h_ != H || !base_settled_) {
            const auto t0 = std::chrono::steady_clock::now();
            const bool old_was_settled = base_settled_;
            const bool old_downscaled = cur_frame_downscaled_;
            prev_base_ = std::move(base_);  // 旧帧移存（供交叉淡入；随后重建 base_）
            if (provisional) {
                // 运动中：低画质快路径——单层不透明光栅（底 + 折线，省略半透明面积层），
                // supersample=1。运动锯齿被 motion 掩盖，换取 60fps；静止再落精美帧。
                termchart::scene::Scene s;
                {
                    termchart::scene::SceneBuilder b(s);
                    renderer_->build_base_scene(b, geom_, glow_, dragging_);
                    renderer_->build_line_scene(b, geom_, glow_);
                }
                const auto t1 = std::chrono::steady_clock::now();
                const int d = anim_downscale(W, H);
                if (d <= 1) {
                    base_ = std::make_unique<Bitmap>(raster(s, W, H, false, 1));
                } else {
                    Bitmap small = raster(s, W / d, H / d, false, 1,
                                          1.0 / static_cast<double>(d));
                    base_ = std::make_unique<Bitmap>(
                        upscale_nearest(small, static_cast<std::uint32_t>(W),
                                        static_cast<std::uint32_t>(H)));
                }
                const auto t2 = std::chrono::steady_clock::now();
                base_settled_ = false;  // 落地后需重建一次精美帧
                cur_frame_downscaled_ = (d > 1);
                if (profiling_) {
                    std::fprintf(stderr,
                                 "frame %d [fast ] scene=%.1fms raster=%.1fms (W=%d H=%d)\n",
                                 profile_frame_++, ms(t1 - t0), ms(t2 - t1), W, H);
                }
            } else {
                // 静止：精美帧——三层光栅 + alpha 合成（底 / 半透明面积 / 折线），supersample=2。
                termchart::scene::Scene sb;
                termchart::scene::Scene sa;
                termchart::scene::Scene sl;
                {
                    termchart::scene::SceneBuilder b(sb);
                    renderer_->build_base_scene(b, geom_, glow_, dragging_);
                }
                {
                    termchart::scene::SceneBuilder b(sa);
                    renderer_->build_area_scene(b, geom_, glow_);
                }
                {
                    termchart::scene::SceneBuilder b(sl);
                    renderer_->build_line_scene(b, geom_, glow_);
                }
                const auto t1 = std::chrono::steady_clock::now();
                const double dpr = pixel_scale_;  // 高 DPI：物理分辨率更高，阈值同步放大
        const int ss = (static_cast<long long>(W) * H >
                        static_cast<long long>(400000) * static_cast<long long>(dpr * dpr))
                           ? 1
                           : 2;
                const Bitmap L0 = raster(sb, W, H, false, ss);
                const Bitmap L1 = raster(sa, W, H, true, ss);
                const Bitmap L2 = raster(sl, W, H, true, ss);
                const auto t2 = std::chrono::steady_clock::now();
                BitmapCanvas comp(static_cast<std::uint32_t>(W),
                                  static_cast<std::uint32_t>(H));
                comp.clear(renderer_->background());
                comp.blit(L0, 0.0, 0.0, 1.0);
                comp.blit(L1, 0.0, 0.0, kAreaOpacity);
                comp.blit(L2, 0.0, 0.0, 1.0);
                base_ = std::make_unique<Bitmap>(comp.snapshot());
                const auto t3 = std::chrono::steady_clock::now();
                base_settled_ = true;
                cur_frame_downscaled_ = false;
                if (!old_was_settled && old_downscaled && prev_base_) {
                    fade_ = 0.0;  // 低清(降分辨率)→高清：交叉淡入
                }
                if (profiling_) {
                    std::fprintf(stderr,
                                 "frame %d [prety] scene=%.1fms raster=%.1fms composite=%.1fms "
                                 "(W=%d H=%d)\n",
                                 profile_frame_++, ms(t1 - t0), ms(t2 - t1), ms(t3 - t2), W, H);
                }
            }
            base_w_ = W;
            base_h_ = H;
        }
        if (fade_ < 1.0 && prev_base_) {
            c.blit(*prev_base_, g.x, g.y);  // 旧低清帧打底
            c.blit(*base_, g.x, g.y, termchart::util::smoothstep(fade_));  // 高清帧淡入铺上
        } else {
            prev_base_.reset();
            c.blit(*base_, g.x, g.y);
        }
        if (hovered_ >= 0 && hovered_ < geom_.n) {
            // 悬停强调（ECharts axisPointer/emphasis）：十字轴线（通用）+ 插头 specifics。
            const std::size_t h = static_cast<std::size_t>(hovered_);
            const double hx = g.x + geom_.xs[h];  // 窗口画布未按控件平移 → 悬停叠层需加全局偏移
            const GuiTheme th = current_theme();
            c.fill_rect(Rect{hx - 0.5, g.y + geom_.pt, 1.0, geom_.pb - geom_.pt},
                        th.crosshair);  // 竖向十字轴线（主题色）
            // 水平十字线 + 右轴实时读数
            const double hyv = renderer_->hover_y(geom_, hovered_);
            if (!std::isnan(hyv)) {
                const double yy = g.y + hyv;
                c.fill_rect(Rect{g.x + geom_.pl, yy, geom_.pr - geom_.pl, 1.0},
                            th.crosshair);  // 横线
                const std::string lab = renderer_->hover_label(geom_, hovered_);
                if (!lab.empty()) {
                    const double lw = static_cast<double>(lab.size()) * 9.0 + 8.0;
                    const double lx = g.x + geom_.pr + 2.0;
                    c.fill_rect(Rect{lx, yy - 8.0, lw, 16.0}, th.tooltip_bg);
                    c.stroke_rect(Rect{lx, yy - 8.0, lw, 16.0}, th.tooltip_border, 1.0);
                    c.draw_text(lx + 4.0, yy - 4.0, lab, th.tooltip_fg);
                }
            }
            renderer_->draw_hover(c, g, geom_, hovered_, glow_);  // 强调（柱/蜡烛高亮）
            renderer_->draw_tooltip(c, g, geom_, hovered_);        // 信息框
        }
    }

    // 交互：悬停（MouseMove）、滚轮缩放（Wheel，Shift+Wheel 缩 y）、拖拽平移（MousePress/Move/Release）。
    bool on_event(const Event& e) override {
        if (!renderer_) return false;
        const Rect g = global_bounds();
        const double hx = e.x - g.x;
        const double hy = e.y - g.y;

        if (e.type == EventType::Wheel) {
            ViewWindow v = renderer_->view();
            const int N = geom_.n;
            if (N <= 0) return false;
            const int hi = (v.hi < 0 || v.hi > N) ? N : v.hi;
            const int lo = std::max(0, v.lo);
            const int curM = (hi - lo) > 0 ? (hi - lo) : N;
            // 光标定到的可见下标
            const double span = (geom_.pr - geom_.pl) > 0 ? (geom_.pr - geom_.pl) : 1.0;
            double frac = std::max(0.0, std::min(1.0, (hx - geom_.pl) / span));
            const int cursor = lo + static_cast<int>(frac * static_cast<double>(curM));
            if (e.modifiers & kModShift) {
                // y 轴缩放（围绕光标 y）
                const double ytop = geom_.ytop;
                const double yspan = geom_.step * geom_.intervals;
                const double ymin0 = ytop - yspan;
                double cmin = view_is_nan(v.ymin) ? ymin0 : v.ymin;
                double cmax = view_is_nan(v.ymax) ? ytop : v.ymax;
                double cspan = cmax - cmin;
                if (!(cspan > 0.0)) cspan = 1.0;
                const double spanY = (geom_.pb - geom_.pt) > 0 ? (geom_.pb - geom_.pt) : 1.0;
                double fy = std::max(0.0, std::min(1.0, (geom_.pb - hy) / spanY));
                const double factor = e.wheel_delta > 0 ? 0.85 : 1.0 / 0.85;
                const double cval = cmin + fy * cspan;
                const double nspan = cspan * factor;
                v.ymin = cval - fy * nspan;
                v.ymax = v.ymin + nspan;
            } else {
                // x 轴缩放（围绕光标）
                const double factor = e.wheel_delta > 0 ? 0.85 : 1.0 / 0.85;
                int newM = static_cast<int>(
                    std::llround(static_cast<double>(curM) * factor));
                newM = std::max(2, std::min(newM, N));
                int newLo =
                    cursor - static_cast<int>(static_cast<double>(cursor - lo) *
                                             static_cast<double>(newM) /
                                             static_cast<double>(curM));
                int newHi = newLo + newM;
                if (newHi > N) {
                    newHi = N;
                    newLo = N - newM;
                }
                if (newLo < 0) {
                    newLo = 0;
                    newHi = newM;
                }
                v.lo = newLo;
                v.hi = (newHi >= N) ? -1 : newHi;
            }
            renderer_->set_view(v);
            dirty_ = true;
            return true;
        }

        if (e.type == EventType::MousePress && e.button == 0) {
            const Rect pg{g.x + geom_.pl, g.y + geom_.pt, geom_.pr - geom_.pl,
                          geom_.pb - geom_.pt};
            if (hx >= pg.x && hx <= pg.x + pg.w && hy >= pg.y && hy <= pg.y + pg.h) {
                drag_active_ = true;
                drag_last_x_ = static_cast<int>(e.x);
                drag_view_ = renderer_->view();
                dragging_ = true;
                return true;
            }
            return false;
        }
        if (e.type == EventType::MouseRelease && e.button == 0) {
            if (drag_active_) {
                drag_active_ = false;
                dragging_ = false;
                snap_pending_ = true;  // 松手吸附
                return true;
            }
            return false;
        }
        if (e.type == EventType::MouseMove) {
            if (drag_active_) {
                const int N = geom_.n;
                ViewWindow v = drag_view_;
                const int hi = (v.hi < 0 || v.hi > N) ? N : v.hi;
                const int lo = std::max(0, v.lo);
                const int M = hi - lo;
                if (M > 0) {
                    const double slot = (geom_.pr - geom_.pl) / static_cast<double>(M);
                    const int shift = static_cast<int>(std::llround(
                        static_cast<double>(e.x - drag_last_x_) / (slot > 0 ? slot : 1.0)));
                    int newLo = lo - shift;
                    int newHi = hi - shift;
                    if (newLo < 0) {
                        newHi -= newLo;
                        newLo = 0;
                    }
                    if (newHi > N) {
                        newLo -= (newHi - N);
                        newHi = N;
                    }
                    if (newLo < 0) newLo = 0;
                    v.lo = newLo;
                    v.hi = (newHi >= N) ? -1 : newHi;
                    renderer_->set_view(v);
                    drag_last_x_ = static_cast<int>(e.x);
                }
                dirty_ = true;
            }
            const int h = renderer_->hit_index(geom_, hx, hy);
            if (h != hovered_) {
                hovered_ = h;
                return true;
            }
            return drag_active_;
        }
        return false;
    }

private:
    // 面积层整体不透明度（半透明合成，让柱子透出来）。
    static constexpr double kAreaOpacity = 0.45;
    // 几何趋近速率（1/秒）：越大跟手越紧、收敛越快。~14 → 时间常数 ~70ms。
    static constexpr double kFollow = 14.0;
    // 高清帧交叉淡入时长（秒）。
    static constexpr double kFadeDur = 0.16;
    // 交互高光衰减速率（1/秒）：5.0 偏慢像"慢渐变"，调大到 9（时间常数 ~110ms）。
    static constexpr double kGlowDecay = 9.0;
    static inline bool profiling_ = false;  // set_profile 切换

    bool drag_active_ = false;  // 拖拽平移进行中
    int drag_last_x_ = 0;       // 上次鼠标 x（窗口坐标）
    ViewWindow drag_view_{};    // 拖拽起始视图窗口
    static double ms(std::chrono::steady_clock::duration d) {
        return std::chrono::duration<double, std::milli>(d).count();
    }
    // 运动中光栅像素预算：超过则按整数降幅 d 降分辨率光栅再放大。
    [[nodiscard]] static int anim_downscale(int W, int H) {
        constexpr double kBudget = 400000.0;  // ~800×500
        const double need =
            std::sqrt(static_cast<double>(W) * static_cast<double>(H) / kBudget);
        const int d = static_cast<int>(std::ceil(need));
        return d < 1 ? 1 : (d > 8 ? 8 : d);
    }

    // 把一个 Scene 光栅化成 Bitmap；transparent=true 时背景 alpha=0（供上层合成）。
    [[nodiscard]] Bitmap raster(const termchart::scene::Scene& sc, int W, int H, bool transparent,
                                int supersample = 2, double scale = 1.0) const {
        termchart::gui::RenderOptions o;
        o.width = static_cast<std::uint32_t>(W);
        o.height = static_cast<std::uint32_t>(H);
        o.autofit = false;
        o.scale = scale;
        o.supersample = supersample;
        o.background = renderer_->background();
        o.foreground = renderer_->foreground();
        const double ps = pixel_scale_;  // 高 DPI：文本格随设备像素比放大，避免发糊
        o.text_cell_w = 9.0 * ps;
        o.text_cell_h = 14.0 * ps;
        o.transparent = transparent;
        termchart::gui::SceneRenderer renderer(o);
        return renderer.render(sc);
    }

    // 最近邻放大（裸指针）：运动中把降分辨率光栅图铺满控件。
    [[nodiscard]] static Bitmap upscale_nearest(const Bitmap& src, std::uint32_t W,
                                                std::uint32_t H) {
        Bitmap out(W, H, 4);
        const std::uint32_t sw = src.width();
        const std::uint32_t sh = src.height();
        if (sw == 0 || sh == 0 || W == 0 || H == 0) {
            return out;
        }
        const std::uint8_t* s = src.data();
        std::uint8_t* d = out.data();
        for (std::uint32_t y = 0; y < H; ++y) {
            const std::uint32_t sy = std::min(
                sh - 1, static_cast<std::uint32_t>(static_cast<std::uint64_t>(y) * sh / H));
            const std::uint8_t* srow = s + static_cast<std::size_t>(sy) * sw * 4;
            std::uint8_t* drow = d + static_cast<std::size_t>(y) * W * 4;
            for (std::uint32_t x = 0; x < W; ++x) {
                const std::uint32_t sx = std::min(
                    sw - 1, static_cast<std::uint32_t>(static_cast<std::uint64_t>(x) * sw / W));
                const std::uint8_t* sp = srow + static_cast<std::size_t>(sx) * 4;
                std::uint8_t* dp = drow + static_cast<std::size_t>(x) * 4;
                dp[0] = sp[0];
                dp[1] = sp[1];
                dp[2] = sp[2];
                dp[3] = sp[3];
            }
        }
        return out;
    }

    std::shared_ptr<IChartRenderer> renderer_;  // 渲染插头（默认 ComboRenderer）
    int hovered_ = -1;
    double pixel_scale_ = 1.0;  // 设备像素比（高 DPI：文本格/事件放大，避免发糊）

    mutable std::unique_ptr<Bitmap> base_;  // 光栅化缓存
    mutable int base_w_ = 0;
    mutable int base_h_ = 0;
    mutable bool base_settled_ = true;  // 缓存是否对应"静止"帧
    mutable int profile_frame_ = 0;     // 插桩帧计数
    mutable ChartGeom geom_{};      // 当前显示几何（悬停命中/绘制都用它）
    mutable ChartGeom target_{};    // 目标几何（resize/数据变化时更新，tick 指数趋近它）
    mutable bool have_geom_ = false;
    mutable bool dirty_ = true;
    mutable bool snap_pending_ = false;  // 下次 paint 吸附到目标（拖动中同步重渲染用）
    mutable std::unique_ptr<Bitmap> prev_base_;  // 交叉淡入的旧帧（低清→高清用）
    mutable double fade_ = 1.0;                 // 高清帧淡入进度（1=完成）
    mutable bool cur_frame_downscaled_ = false;  // 当前帧是否降分辨率光栅（决定松手是否淡入）
    mutable double glow_ = 0.0;                 // 交互高光强度（0=基色，1=高亮）
    mutable bool dragging_ = false;      // 拖动中：冻结标签抽稀步长（只调柱宽/间距）
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_CHART_WIDGET_HPP
