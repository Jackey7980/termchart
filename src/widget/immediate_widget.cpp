// immediate_widget.cpp —— 无状态即时组件实现（无终端依赖，只写 Canvas）
#include "termchart/widget/immediate_widget.hpp"

#include <cstddef>

namespace termchart::widget {
namespace {

// 返回 s 的前 max_cp 个完整 UTF-8 码点占用的字节数。
// 非法首字节按 1 字节吞掉保证前进；截断的字节序列停在边界前。
std::size_t utf8_take(std::string_view s, std::size_t max_cp) noexcept {
    std::size_t cp = 0;
    std::size_t i = 0;
    while (i < s.size() && cp < max_cp) {
        const auto lead = static_cast<unsigned char>(s[i]);
        std::size_t len = 1;
        if ((lead & 0xE0u) == 0xC0u) {
            len = 2;
        } else if ((lead & 0xF0u) == 0xE0u) {
            len = 3;
        } else if ((lead & 0xF8u) == 0xF0u) {
            len = 4;
        }
        if (i + len > s.size()) {
            break;  // 末尾被截断的序列：不越界取
        }
        i += len;
        ++cp;
    }
    return i;
}

// 统一报告规则：意图只由输入旗标决定（见 immediate_widget.hpp 头注）。
bool intent_of(const ControlState& state) noexcept {
    return state.enabled && state.hovered && state.pressed;
}

}  // namespace

bool button_clicked(canvas::Canvas& canvas, Rect rect, std::string_view label,
                    const ThemePalette& theme, const ControlState& state) noexcept {
    const bool clicked = intent_of(state);
    if (rect.empty()) {
        return clicked;  // 画布外契约：退化几何只跳过绘制
    }

    Style style = style_fg_bg(theme.foreground, theme.background);
    if (!state.enabled) {
        style = style_fg_bg(theme.text_dim, theme.background);
    } else if (state.hovered && state.pressed) {
        style = style_fg_bg(theme.background, theme.accent);  // 按压反白
    } else if (state.hovered) {
        style = style_fg_bg(theme.foreground, theme.highlight_bg);
    } else if (state.focused) {
        style = style_fg_bg(theme.accent, theme.background);
        style.set_attr(Attr::Bold);
    }

    canvas.fill_rect(rect, U' ', style);

    // 标签：左右各留 1 格内边距，水平居中、垂直居中，按码点截断。
    const double inset = 1.0;
    const double avail_w = rect.w - 2.0 * inset;
    if (avail_w >= 1.0) {
        const auto max_cp = static_cast<std::size_t>(avail_w);
        const std::string_view shown = label.substr(0, utf8_take(label, max_cp));
        std::size_t cp = 0;
        for (std::size_t i = 0; i < shown.size();) {
            const auto lead = static_cast<unsigned char>(shown[i]);
            i += ((lead & 0xE0u) == 0xC0u)   ? 2
                 : ((lead & 0xF0u) == 0xE0u) ? 3
                 : ((lead & 0xF8u) == 0xF0u) ? 4
                                             : 1;
            ++cp;
        }
        const double x0 = rect.x + inset + (avail_w - static_cast<double>(cp)) / 2.0;
        const double y0 = rect.y + (rect.h - 1.0) / 2.0;
        canvas.draw_text(Point{x0, y0}, shown, style);
    }
    return clicked;
}

bool checkbox_toggled(canvas::Canvas& canvas, Rect rect, std::string_view label, bool checked,
                      const ThemePalette& theme, const ControlState& state) noexcept {
    const bool toggled = intent_of(state);
    // `[x]` 至少要 3 格、1 行才画得出；更退化的几何整体跳过绘制。
    if (rect.w < 3.0 || rect.h < 1.0) {
        return toggled;
    }

    const Rgb dim = state.enabled ? theme.border : theme.text_dim;
    const Style frame_style = style_fg(dim);
    const Style mark_style =
        style_fg(state.enabled ? (checked ? theme.accent : theme.border) : theme.text_dim);
    Style text_style = style_fg(state.enabled ? theme.foreground : theme.text_dim);
    if (state.focused) {
        text_style.set_attr(Attr::Bold);
    }

    const double bx = rect.x;
    const double y0 = rect.y;
    canvas.put(Point{bx, y0}, U'[', frame_style);
    canvas.put(Point{bx + 1.0, y0}, checked ? U'x' : U' ', mark_style);
    canvas.put(Point{bx + 2.0, y0}, U']', frame_style);

    const double label_x = bx + 4.0;  // "] " 之后
    const double avail_w = rect.x + rect.w - label_x;
    if (avail_w >= 1.0) {
        const auto max_cp = static_cast<std::size_t>(avail_w);
        canvas.draw_text(Point{label_x, y0}, label.substr(0, utf8_take(label, max_cp)),
                         text_style);
    }
    return toggled;
}

}  // namespace termchart::widget
