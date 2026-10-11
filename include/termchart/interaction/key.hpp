// key.hpp —— 按键定义（header-only 值类型）
#ifndef TERMCHART_INTERACTION_KEY_HPP
#define TERMCHART_INTERACTION_KEY_HPP

#include <cstdint>

namespace termchart::interaction {

enum class Key {
    Unknown = 0,
    Char,       // 可打印字符，具体码点在 KeyEvent::ch
    Enter,
    Tab,
    Backspace,
    Escape,
    Up,
    Down,
    Left,
    Right,
    Home,
    End,
    PageUp,
    PageDown,
    Insert,
    Delete,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
};

using ModMask = std::uint8_t;

namespace mod {
inline constexpr ModMask kNone = 0;
inline constexpr ModMask kShift = 1u << 0;
inline constexpr ModMask kAlt = 1u << 1;
inline constexpr ModMask kCtrl = 1u << 2;
}  // namespace mod

[[nodiscard]] constexpr bool has_mod(ModMask mask, ModMask bit) noexcept {
    return (mask & bit) != 0;
}

struct KeyEvent {
    Key key = Key::Unknown;
    char32_t ch = 0;             // key == Char 时有效
    ModMask mods = mod::kNone;
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_KEY_HPP
