// event.hpp —— 输入事件变体（header-only）
//
// Event 用 std::variant 表达"按键 / 鼠标 / 尺寸 / 焦点 / 粘贴"五类事件；
// variant 的 index() 即 EventKind，与 EventKind 的枚举顺序一一对应。
#ifndef TERMCHART_INTERACTION_EVENT_HPP
#define TERMCHART_INTERACTION_EVENT_HPP

#include <cstdint>
#include <string>
#include <variant>

#include "termchart/interaction/key.hpp"
#include "termchart/interaction/mouse.hpp"

namespace termchart::interaction {

struct ResizeEvent {
    int cols = 0;
    int rows = 0;
};

struct FocusEvent {
    bool focused = true;
};

struct PasteEvent {
    std::string text;
};

enum class EventKind : std::uint8_t {
    Key = 0,
    Mouse,
    Resize,
    Focus,
    Paste,
};

using Event = std::variant<KeyEvent, MouseEvent, ResizeEvent, FocusEvent, PasteEvent>;

[[nodiscard]] constexpr EventKind kind_of(const Event& event) noexcept {
    return static_cast<EventKind>(event.index());
}

[[nodiscard]] constexpr const char* kind_name(EventKind kind) noexcept {
    switch (kind) {
        case EventKind::Key:
            return "key";
        case EventKind::Mouse:
            return "mouse";
        case EventKind::Resize:
            return "resize";
        case EventKind::Focus:
            return "focus";
        case EventKind::Paste:
            return "paste";
    }
    return "unknown";
}

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_EVENT_HPP
