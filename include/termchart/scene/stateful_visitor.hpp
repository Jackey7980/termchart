// stateful_visitor.hpp —— 有状态 Scene 消费者基类
//
// 把"Fill/Stroke 是状态指令、路径需累积、Rect 有填充/描边分支"这套时序
// 收在一处。派生类只实现"往哪画"，不管"什么时候画"。
//
// 为什么需要它：canvas 的 ScenePainter 与 backend/vector 的 SVG 后端各自
// 写了一遍同一套状态机（MoveTo/LineTo 累积、遇到状态指令先按旧状态收口、
// Rect 按有无填充色分两支）。两处独立演化就会出现"同一份 Scene、两种画法"
// 的偏差；这里是那个偏差的唯一收敛点。
//
// 时序约定（与 svg_backend / scene_renderer 原行为一致）：
//   * MoveTo     —— 更新 current_，不 flush（允许多子路径合成一条 <path>）；
//   * LineTo     —— 首个 LineTo 触发 begin_path，其余 extend_path；
//   * Fill/Stroke—— 先 end_path() 收口旧状态，再更新；重置 path_open_；
//   * Rect/Arc/Text/Image —— 先 end_path() 收口，再 emit，并重置 path_open_；
//   * run() 结束 —— 收尾一次。
//
// 两条容易写错的约束（写错就是静默错图，不是编译错误）：
//   1. end_path() 必须**无条件**调用，不能在基类里加 `if (path_open_)` 守卫：
//      MoveTo 会把 path_open_ 置 false，但派生类累积的几何可能已经 dirty，
//      一守卫就漏掉这次收口。判空交给实现自己（d_.empty() / dirty 标志）。
//   2. end_path() 里读到的 fill_/stroke_ 是**旧值**——因为 FillOp/StrokeOp
//      的处理顺序是「先收口、后赋值」。这正是想要的：几何按它出现时的状态着色。
#ifndef TERMCHART_SCENE_STATEFUL_VISITOR_HPP
#define TERMCHART_SCENE_STATEFUL_VISITOR_HPP

#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"
#include "termchart/scene/visitor.hpp"

namespace termchart::scene {

class StatefulVisitor : public CommandVisitor {
public:
    void visit(const MoveTo& command) override;
    void visit(const LineTo& command) override;
    void visit(const RectOp& command) override;
    void visit(const ArcOp& command) override;
    void visit(const FillOp& command) override;
    void visit(const StrokeOp& command) override;
    void visit(const TextOp& command) override;
    void visit(const ImageOp& command) override;
    // 分组不参与状态机：不触发收口、不改变当前 fill/stroke、不开也不闭路径。
    // 显式 override（而非依赖基类默认）一是消掉 -Woverloaded-virtual，二是
    // 表明"这里是有意忽略"，不是忘了实现。
    void visit(const GroupOp& command) override;

    // 2026-10-26 扩编指令的状态机接入。这些 visit 只做"状态登记 / 路径管理"，
    // 渲染由各后端 override 的 hook 完成；旧后端（PDF/GUI）继承 CommandVisitor
    // 的默认空 visit 即编译通过，行为零影响。
    void visit(const CurveTo& command) override;
    void visit(const PushTransformOp& command) override;
    void visit(const PopTransformOp& command) override;
    void visit(const ClipOp& command) override;
    void visit(const BeginPathOp& command) override;
    void visit(const ClosePathOp& command) override;
    void visit(const LinearGradientOp& command) override;
    void visit(const RadialGradientOp& command) override;
    void visit(const GlobalAlphaOp& command) override;
    void visit(const DashOp& command) override;
    void visit(const SetLineWidthOp& command) override;
    void visit(const SetStrokeCapOp& command) override;
    void visit(const BlendModeOp& command) override;

    // 遍历 + 收尾。调用方不应再自己调 end_path。
    void run(const Scene& scene);

protected:
    // ── 派生类实现的"往哪画" ──
    // begin_path/extend_path/end_path 三条是路径的三段式：begin 发起点、
    // extend 追加一段、end 收口。SVG 需要 begin（要发 M），终端不需要
    // （extend_path 自带两端），因此派生类把它实现成 no-op 即可。
    virtual void begin_path(const Point& from) = 0;
    virtual void extend_path(const Point& from, const Point& to) = 0;
    virtual void end_path() = 0;

    virtual void emit_filled_rect(const Rect& rect, const Color& fill) = 0;
    virtual void emit_stroked_rect(const Rect& rect, const Color& stroke, double width) = 0;
    virtual void emit_arc(const ArcOp& arc, const Color& stroke, double width) = 0;
    virtual void emit_text(const TextOp& text) = 0;
    virtual void emit_image(const ImageOp& image) = 0;

    // 扩编 hook（默认空：PDF/GUI 等未实现的後端继承即可编译；Canvas/SVG override）
    // 曲线段：from=当前点，c1/c2 控制点，to 终点（坐标已由基类变换）。
    virtual void extend_path_curve(const Point& from, const Point& c1, const Point& c2,
                                  const Point& to) {}
    // 闭合当前子路径：Canvas 已由基类用直线连回起点，无需额外动作；SVG 发 'Z'。
    virtual void close_path() {}
    // 渐变填充（作用于其后 Fill / 带填充色的 RectOp）：rect 为已变换矩形。
    // 默认回退：不支持渐变的后端（PDF/GUI）用首色实色填充，避免矩形消失。
    virtual void emit_filled_rect_gradient(const Rect& rect, bool radial, const Point& g0,
                                          const Point& g1, double radius,
                                          const std::vector<GradientStop>& stops) {
        (void)radial;
        (void)g0;
        (void)g1;
        (void)radius;
        const Rgb c = stops.empty() ? Rgb{0, 0, 0} : stops.front().color;
        emit_filled_rect(rect, Color::rgb(c));
    }

    // ── 当前状态只读视图（end_path 里读的是"旧值"，见文件头第 2 条）──
    [[nodiscard]] const Color& current_fill() const noexcept { return fill_; }
    [[nodiscard]] const Color& current_stroke() const noexcept { return stroke_; }
    [[nodiscard]] double current_stroke_width() const noexcept { return stroke_width_; }
    [[nodiscard]] double current_alpha() const noexcept { return alpha_; }
    [[nodiscard]] const DashOp* current_dash() const noexcept {
        return dash_active_ ? &dash_ : nullptr;
    }
    [[nodiscard]] StrokeCap current_cap() const noexcept { return cap_; }
    [[nodiscard]] BlendMode current_blend() const noexcept { return blend_; }
    [[nodiscard]] const Rect* clip_rect() const noexcept {
        return clips_.empty() ? nullptr : &clip_composed_;
    }
    [[nodiscard]] bool fill_is_gradient() const noexcept { return fill_is_gradient_; }
    void fill_gradient_params(bool& radial, Point& g0, Point& g1, double& radius,
                             const std::vector<GradientStop>*& stops) const noexcept {
        radial = fill_grad_radial_;
        g0 = fill_grad_g0_;
        g1 = fill_grad_g1_;
        radius = fill_grad_radius_;
        stops = &fill_stops_;
    }

    // 坐标变换（应用当前 transform 栈）：派生类在 emit 前把用户坐标映射到画布坐标。
    [[nodiscard]] Point transform_point(const Point& p) const noexcept;
    [[nodiscard]] Rect transform_rect(const Rect& r) const noexcept;

private:
    void finish_path();  // end_path + 重置 path_open_

    // 状态（默认值均恒等，不影响旧 Op 行为）
    Color fill_ = Color::none();
    Color stroke_ = Color::none();
    double stroke_width_ = 1.0;
    Point current_{};
    bool path_open_ = false;
    Point subpath_start_{};

    std::vector<Transform> transforms_;  // 变换栈（空=恒等）
    Rect clip_composed_{};               // 当前裁剪区（clips_ 非空时有效）
    std::vector<Rect> clips_;            // 裁剪栈（intersect 累积；!intersect 覆盖）
    double alpha_ = 1.0;
    DashOp dash_;
    bool dash_active_ = false;
    StrokeCap cap_ = StrokeCap::Butt;
    BlendMode blend_ = BlendMode::Normal;

    bool fill_is_gradient_ = false;
    bool fill_grad_radial_ = false;
    Point fill_grad_g0_{};
    Point fill_grad_g1_{};
    double fill_grad_radius_ = 0.0;
    std::vector<GradientStop> fill_stops_;
};

}  // namespace termchart::scene

#endif  // TERMCHART_SCENE_STATEFUL_VISITOR_HPP
