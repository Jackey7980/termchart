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

    // ── 当前状态只读视图（end_path 里读的是"旧值"，见文件头第 2 条）──
    [[nodiscard]] const Color& current_fill() const noexcept { return fill_; }
    [[nodiscard]] const Color& current_stroke() const noexcept { return stroke_; }
    [[nodiscard]] double current_stroke_width() const noexcept { return stroke_width_; }

private:
    void finish_path();  // end_path + 重置 path_open_

    Color fill_ = Color::none();
    Color stroke_ = Color::none();
    double stroke_width_ = 1.0;
    Point current_{};
    bool path_open_ = false;
};

}  // namespace termchart::scene

#endif  // TERMCHART_SCENE_STATEFUL_VISITOR_HPP
