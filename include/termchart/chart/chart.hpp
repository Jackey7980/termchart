// chart.hpp —— Chart 抽象基类
//
// chart/ 是 stage 3 的顶点：把数据系列 + 比例尺 + layout 拼成可见图表，画进
// canvas::Canvas。**不碰 render 层**（架构禁止边），于是未来 to_document()
// 通道独立存在。绘制上下文见 context.hpp。
#ifndef TERMCHART_CHART_CHART_HPP
#define TERMCHART_CHART_CHART_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/animation.hpp"
#include "termchart/chart/context.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/scale/auto_domain.hpp"

namespace termchart::scene {
class Scene;
}

namespace termchart::layout {
class Sink;
}

namespace termchart::chart {

// `AnimationTrack` / `AnimationState` 在 chart/animation.hpp 里定义（别名
// `chart::Easing` 也在那里），这里只负责持有"名字 → 状态"的表。

// 折线/柱共享的字符与几何辅助（定义见 chart.cpp）。
inline constexpr char32_t kLineChar = U'\u2022';     // • 连线
inline constexpr char32_t kMarkerChar = U'\u25CF';   // ● 数据点
inline constexpr char32_t kBarChar = U'\u2588';      // █ 柱

// 默认配色的唯一真源在 `chart/series.hpp`（`DefaultColor` / `default_color()`）——
// 放在那里是因为 chart.hpp 自己 include series.hpp，反过来不行。

void data_bounds(const std::vector<Series>& series, double& xmin, double& xmax,
                  double& ymin, double& ymax);
// y_override：显式 Y 域（**仅 Y**，X 仍按 auto_domain 规整）。给"轴域不该取
// 极值"的场景用——离群裁剪（autoviz 的稳健域）、固定域对比图。
// 有值时它是权威值：不再 nice_domain、不因 include_zero 扩域，**include_zero
// 由调用方自己并进域里**（柱状图必须含 0，见 BarChart::set_y_domain）。
// 钳制由比例尺完成（LinearScale 默认 clamp=true），域外的点自动落在轴边缘。
layout::PlotArea make_area(canvas::Canvas& canvas, const layout::Margin& margin,
                           double xmin, double xmax, double ymin, double ymax,
                           bool auto_domain, bool include_zero,
                           std::optional<scale::Domain> y_override = std::nullopt);

// 尺寸版：布局只依赖目标宽高，不依赖那个"画布对象"本身。
// 存在的理由：to_scene() 没有 Canvas（Scene 是无限画布），但仍要算同一套
// 网格位置。Canvas 版内部转发到这里，两版不可能算出不同的布局。
layout::PlotArea make_area(std::uint32_t width, std::uint32_t height,
                           const layout::Margin& margin, double xmin, double xmax,
                           double ymin, double ymax, bool auto_domain, bool include_zero,
                           std::optional<scale::Domain> y_override = std::nullopt);
void draw_legend_for(canvas::Canvas& canvas, const std::vector<Series>& series,
                     const layout::PlotArea& area, bool show);
// Sink 版：图例的排布（间距、起始列、色块字形）只有一份，落笔交给 sink。
void draw_legend_for(layout::Sink& sink, const std::vector<Series>& series,
                     const layout::PlotArea& area, bool show);

// "哪一组数据画到几成"：Scene 里的组 id ↔ 进度。
// 它是 Chart 与 `apply_animation`（见 chart/scene_animation.hpp）之间的桥：
// Chart 知道"组 0 是收入线"，Scene 只知道"组 0"，中间这张表补上语义。
struct GroupProgress {
    std::uint32_t group = 0;
    double progress = 1.0;  // [0,1]
};

class Chart {
public:
    virtual ~Chart();
    virtual void draw(DrawContext ctx) = 0;

    // ── 真实 Scene 输出 ──────────────────────────────────────────────
    // 把图表描述成矢量指令（MoveTo/LineTo/StrokeOp/TextOp/GroupOp），而不是
    // 直接往字符格里落笔。终端的盲文/半块、SVG 的 <path>、PDF 的描边，都是
    // 对同一份 Scene 的不同解释——不再是"先画完字符再转录"。
    //
    // width/height 是**栅格化目标**的尺寸：Scene 本身没有尺寸，但"第几列
    // 是刻度"这件事依赖目标宽高，所以必须显式给。两处传同一个值即可保证
    // 与 draw() 的布局完全一致。
    //
    // 返回 false = 该图种尚未迁移，调用方应回退到 draw(Canvas)。
    // 默认实现返回 false，因此老图种零改动、行为不变。
    [[nodiscard]] virtual bool to_scene(scene::Scene& out, std::uint32_t width,
                                        std::uint32_t height) const;

    void set_title(std::string title) { title_ = std::move(title); }
    [[nodiscard]] const std::string& title() const noexcept { return title_; }

    // ── 按名动画 ────────────────────────────────────────────────────
    // 只登记"这一段该多久、怎么缓"，不碰 Scene、不碰渲染管线。
    // 具体怎么用进度（截断折线 / 柱高生长）由 `apply_animation` 决定，
    // 而不是散在每个图种的绘制代码里——那样每加一个图种就要重写一遍动画。
    // 同名重复登记 = 覆盖。
    void add_animation(std::string name, AnimationTrack track);
    void add_animation(std::string name, double duration_seconds,
                       Easing easing = Easing::EaseOutCubic, double delay = 0.0);
    void clear_animations() noexcept;
    [[nodiscard]] std::size_t animation_count() const noexcept { return tracks_.size(); }

    // 推进时钟。**第一次调用即动画起点**（不需要单独的 start()）：
    // 之后每次 tick 传入"现在"，内部差分成 dt 再喂给每个 `AnimationState`。
    // 选中绝对时刻而不是只收 dt，是因为暂停/续播/跳转要的是"对齐到外部时钟"；
    // 需要纯增量步进的调用方直接用 `AnimationState::tick(dt)`。
    // 非有限值直接忽略，不污染时间轴。
    void tick(double now_seconds) noexcept;
    // 把当前时刻当作新的起点重放（交互里"重新播放"按钮用）。
    void restart_animations(double now_seconds) noexcept;

    // 该组当前进度，[0,1]。
    // 两种情况返回 1.0（完全显示，等于没有动画）：
    //   1. 这个名字没登记过；
    //   2. 登记过但从未 tick（静态渲染路径不该被动画拖成空白）。
    [[nodiscard]] double progress_of(const std::string& name) const noexcept;
    // 是否还有未跑完的组（渲染循环据此决定要不要继续出帧）。
    [[nodiscard]] bool animating() const noexcept;

    // 把"组 id → 当前进度"写进 out（**不追加到已有内容之外**：调用方先 clear）。
    // 默认空实现 = 该图种不做动画，`apply_animation` 拿到空表就原样返回。
    // 组 id 的分配口径由图种自己定，但**必须与 to_scene 里 begin_group 的编号一致**。
    virtual void animation_groups(std::vector<GroupProgress>& out) const;

    void set_margins(double top, double right, double bottom, double left) {
        margin_top_ = top;
        margin_right_ = right;
        margin_bottom_ = bottom;
        margin_left_ = left;
    }
    [[nodiscard]] double margin_top() const noexcept { return margin_top_; }
    [[nodiscard]] double margin_right() const noexcept { return margin_right_; }
    [[nodiscard]] double margin_bottom() const noexcept { return margin_bottom_; }
    [[nodiscard]] double margin_left() const noexcept { return margin_left_; }

protected:
    std::string title_;
    double margin_top_ = 1.0;
    double margin_right_ = 1.0;
    double margin_bottom_ = 3.0;
    double margin_left_ = 8.0;

private:
    // 存状态而不是配置：`tick(now)` 把 now 差分成 dt 累积进每个 state，
    // "改配置"仍需走 add_animation（它会用新 AnimationTrack 重建状态）。
    std::unordered_map<std::string, AnimationState> tracks_;
    double clock_origin_ = 0.0;  // 首帧时刻 = 动画起点
    double clock_now_ = 0.0;
    bool ticking_ = false;       // 从未 tick → 一律视为"已完成"
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CHART_HPP
