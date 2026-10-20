// native_loop/event.hpp —— GUI 事件模型
//
// GUI 平行栈自己写事件循环（不借终端 loop）。事件是一个扁平结构，按 type 区分
// 载荷；足够驱动 widget_tree 的命中测试与绘制请求，将来接真实 tk 时由
// backend_tk/tk_event_map 把原生事件翻译成它。
#ifndef TERMCHART_GUI_NATIVE_LOOP_EVENT_HPP
#define TERMCHART_GUI_NATIVE_LOOP_EVENT_HPP

#include <cstdint>
#include <string>

namespace termchart_gui {

enum class EventType {
    Paint,        // 请求重绘
    MousePress,
    MouseRelease,
    MouseMove,
    Wheel,        // 滚轮（图表缩放）；delta 为正=向上/向前
    KeyPress,
    Resize,
    Timer,        // 定时器到期
    Quit,
};

// 修饰键位（Event.modifiers）：对应主流工具包的 Shift/Ctrl/Alt。
enum EventModifier : std::uint32_t {
    kModShift = 1u << 0,
    kModCtrl = 1u << 1,
    kModAlt = 1u << 2,
};

struct Event {
    EventType type = EventType::Paint;
    // 鼠标
    double x = 0.0;
    double y = 0.0;
    int button = 0;
    double wheel_delta = 0.0;  // Wheel：格数或行数（后端约定，±120 制或 ±1 制）
    std::uint32_t modifiers = 0;  // EventModifier 位组合
    // 键盘
    int key = 0;
    std::string text;
    // 尺寸
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // 定时器
    std::uint64_t timer_id = 0;
    // 来源（原生后端可填）
    void* source = nullptr;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_NATIVE_LOOP_EVENT_HPP
