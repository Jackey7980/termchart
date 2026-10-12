// immediate.hpp —— 即时模式绑定（Event 流 → 交互组件的最小路由层）
//
// 职责切分（为什么单开一个文件）：
//   * 只做路由与簿记：维护最近鼠标位置与按键按下集合，把 Event 流翻译成
//     Viewport（滚轮缩放 + 拖拽平移）与 BoxSelect（框选三段）的状态变更。
//     不持有终端、不绘制、不认识图表类型；组件状态都在调用方手里
//     （参照 Dear ImGui 的"每帧重放"：本类自身不沉淀任何图表状态）。
//   * 复用既有组件而非重写：平移语义交给 PanGesture（drag_into 的
//     "内容跟随指针"与未拖拽 no-op 被原样尊重），框选三段交给
//     BoxSelect（select.hpp，begin/update/end），本文件只补"按键归属
//     哪个手势"的判定。
//
// 滚轮策略（为什么用滚轮动作而不是 Key '+'/'-'）：
//   MouseEvent 没有 wheel 数值字段，但有 MouseAction::ScrollUp/ScrollDown
//   —— 它们就是滚轮事件本身，比 Key 事件 '+'/'-' 更贴近 wheel_step
//   "滚轮缩放因子"的语义，故采用滚轮动作：
//     * ScrollUp   → zoom_at(+wheel_step, 鼠标位置)   （放大，看得更细）
//     * ScrollDown → zoom_at(1/wheel_step, 鼠标位置)  （缩小，回看全局）
//
// 按键编号：以 mouse.hpp 的 MouseButton 为准（None=0/Left=1/Middle=2/
// Right=3）。bindings 的缺省值是刻意的：pan_button=1（左键拖拽平移），
// select_button=0（= None，即**缺省不启用框选**）——两者都缺省到左键会
// 让拖拽与框选互相打架，调用方要框选须显式给 select_button 分配按键
// （通常右键 3）。button == None 的 Press/Release 事件被整体忽略。
//
// 坐标系约定：feed() 不做"终端格坐标 → 数据坐标"换算，鼠标格坐标被原样
// 当作数据坐标喂给 Viewport/BoxSelect（与 pan.hpp 的纯几何约定一致）；
// 两者尺度不同时由调用方先行换算。
//
// 不变量：
//   * mouse() 是最近一次鼠标事件的坐标（未收到过为 {0,0}）；
//   * 按键按下集合只由 Press/Release 翻转，Move/Scroll 不改变按键状态；
//   * 非鼠标事件（Key/Resize/Focus/Paste）不改变任何状态。
//
// 退化输入：
//   * wheel_step 非有限或 <= 0：滚轮事件被整体忽略（不调用 zoom_at，
//     即使 zoom_at 自身也能拒绝这些值——双保险，见契约"非有限值不调用"）；
//   * 未按下对应按键时，Move 不驱动视口/框选（PanGesture::drag_to 返回
//     nullopt、BoxSelect::update no-op 的语义被尊重）。
#ifndef TERMCHART_INTERACTION_IMMEDIATE_HPP
#define TERMCHART_INTERACTION_IMMEDIATE_HPP

#include "termchart/core/point.hpp"
#include "termchart/interaction/event.hpp"
#include "termchart/interaction/mouse.hpp"
#include "termchart/interaction/pan.hpp"
#include "termchart/interaction/select.hpp"
#include "termchart/interaction/zoom.hpp"
#include "termchart/export.hpp"

namespace termchart::interaction {

struct ImmediateBindings {
    int pan_button = 1;        // 拖拽平移使用的鼠标键（MouseButton 编号）
    int select_button = 0;     // 框选使用的鼠标键；0 = None，缺省不启用框选
    double wheel_step = 1.25;  // 滚轮缩放因子（ScrollUp 放大 / ScrollDown 缩小）
};

class ImmediateContext {
public:
    explicit ImmediateContext(ImmediateBindings bindings = {}) noexcept;

    // 喂一个事件：内部维护鼠标位置/按键状态，并驱动 viewport（滚轮缩放 +
    // 拖拽平移）与 select（框选三段：按下/拖动/松开）。
    void feed(const Event& event, Viewport& viewport, BoxSelect& select);

    [[nodiscard]] Point mouse() const noexcept;  // 最近一次鼠标位置（未收到过为 {0,0}）
    [[nodiscard]] bool dragging() const noexcept;  // 平移拖拽进行中
    [[nodiscard]] const ImmediateBindings& bindings() const noexcept;

private:
    void handle_mouse(const MouseEvent& ev, Viewport& viewport, BoxSelect& select) noexcept;
    // 以当前鼠标位置为锚做一次滚轮缩放；factor 非有限或 <= 0 时忽略。
    void zoom_wheel(double factor, Viewport& viewport) noexcept;

    ImmediateBindings bindings_{};
    Point mouse_{};         // 最近鼠标位置
    unsigned pressed_ = 0;  // 按键按下集合，bit n = MouseButton 编号 n
    PanGesture pan_{};
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_IMMEDIATE_HPP
