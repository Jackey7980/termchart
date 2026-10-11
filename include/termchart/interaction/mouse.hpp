// mouse.hpp —— 鼠标事件（header-only 值类型）
#ifndef TERMCHART_INTERACTION_MOUSE_HPP
#define TERMCHART_INTERACTION_MOUSE_HPP

#include "termchart/interaction/key.hpp"

namespace termchart::interaction {

enum class MouseButton {
    None = 0,
    Left,
    Middle,
    Right,
};

enum class MouseAction {
    Move = 0,
    Press,
    Release,
    ScrollUp,
    ScrollDown,
};

struct MouseEvent {
    MouseAction action = MouseAction::Move;
    MouseButton button = MouseButton::None;
    int x = 0;  // 1-based 终端列
    int y = 0;  // 1-based 终端行
    ModMask mods = mod::kNone;
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_MOUSE_HPP
