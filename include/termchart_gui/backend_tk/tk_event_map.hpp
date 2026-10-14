// backend_tk/tk_event_map.hpp —— 原生事件 → Event 的映射工具
//
// 真实后端用 OS 事件结构填充这些构造器；无头后端直接构造 Event。集中在这里，使
// 映射逻辑只写一处（平行栈不碰终端 interaction 的事件模型）。
#ifndef TERMCHART_GUI_BACKEND_TK_TK_EVENT_MAP_HPP
#define TERMCHART_GUI_BACKEND_TK_TK_EVENT_MAP_HPP

#include "termchart_gui/fwd.hpp"
#include "termchart_gui/native_loop/event.hpp"

namespace termchart_gui {
namespace tk_event_map {

[[nodiscard]] inline Event mouse_press(double x, double y, int button = 0) {
    Event e;
    e.type = EventType::MousePress;
    e.x = x;
    e.y = y;
    e.button = button;
    return e;
}

[[nodiscard]] inline Event mouse_release(double x, double y, int button = 0) {
    Event e;
    e.type = EventType::MouseRelease;
    e.x = x;
    e.y = y;
    e.button = button;
    return e;
}

[[nodiscard]] inline Event mouse_move(double x, double y) {
    Event e;
    e.type = EventType::MouseMove;
    e.x = x;
    e.y = y;
    return e;
}

[[nodiscard]] inline Event key_press(int key, std::string text = {}) {
    Event e;
    e.type = EventType::KeyPress;
    e.key = key;
    e.text = std::move(text);
    return e;
}

[[nodiscard]] inline Event resize(std::uint32_t w, std::uint32_t h) {
    Event e;
    e.type = EventType::Resize;
    e.width = w;
    e.height = h;
    return e;
}

[[nodiscard]] inline Event timer(std::uint64_t id) {
    Event e;
    e.type = EventType::Timer;
    e.timer_id = id;
    return e;
}

[[nodiscard]] inline Event quit() {
    Event e;
    e.type = EventType::Quit;
    return e;
}

}  // namespace tk_event_map
}  // namespace termchart_gui

#endif  // TERMCHART_GUI_BACKEND_TK_TK_EVENT_MAP_HPP
