// controls.cpp —— 无状态即时组件实现（下拉框 / 文本框 / 单选组）
//
// 绘制约定与 button/checkbox 同源：全部只写 canvas（无终端依赖）；意图只由
// 输入决定；退化几何跳过绘制。UTF-8 一律按码点操作。
#include "termchart/widget/controls.hpp"

#include <algorithm>
#include <cstddef>

namespace termchart::widget {
namespace {

// s 的前 max_cp 个完整 UTF-8 码点占用的字节数（与 immediate_widget.cpp 同口径）。
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
            break;
        }
        i += len;
        ++cp;
    }
    return i;
}

// 从字节偏移 i 起跳一个码点，返回新偏移；i 不在码点边界时向前找边界。
std::size_t utf8_next(std::string_view s, std::size_t i) noexcept {
    if (i >= s.size()) {
        return s.size();
    }
    const auto lead = static_cast<unsigned char>(s[i]);
    std::size_t len = 1;
    if ((lead & 0xE0u) == 0xC0u) {
        len = 2;
    } else if ((lead & 0xF0u) == 0xE0u) {
        len = 3;
    } else if ((lead & 0xF8u) == 0xF0u) {
        len = 4;
    }
    if ((lead & 0xC0u) == 0x80u) {
        len = 1;  // 续字节落点：按单字节前进兜底
    }
    return std::min(i + len, s.size());
}

// 从字节偏移 i 起退一个码点（跳过续字节），返回新偏移。
std::size_t utf8_prev(std::string_view s, std::size_t i) noexcept {
    if (i == 0) {
        return 0;
    }
    std::size_t j = i - 1;
    while (j > 0 && (static_cast<unsigned char>(s[j]) & 0xC0u) == 0x80u) {
        --j;
    }
    return j;
}

std::size_t codepoints_before(std::string_view s, std::size_t byte_offset) noexcept {
    std::size_t cp = 0;
    for (std::size_t i = 0; i < byte_offset && i < s.size();) {
        i = utf8_next(s, i);
        ++cp;
    }
    return cp;
}

// 夹取到 [0, n)。
std::size_t clamp_index(std::size_t value, std::size_t n) noexcept {
    return n == 0 ? 0 : std::min(value, n - 1);
}

// 展开清单的开窗：候选项多于可视行时，窗口随 active 居中并保证其在窗内。
// 纯函数：意图映射与绘制共用，保证"点哪行 = 看到的那一行"。
std::size_t dropdown_window_first(std::size_t item_count, std::size_t visible_rows,
                                  std::size_t active) noexcept {
    if (item_count <= visible_rows || visible_rows == 0) {
        return 0;
    }
    std::size_t first = active >= visible_rows / 2 ? active - visible_rows / 2 : 0;
    if (first + visible_rows > item_count) {
        first = item_count - visible_rows;
    }
    return first;
}

}  // namespace

// ── 下拉框 ────────────────────────────────────────────────────────────────

DropdownResult dropdown(canvas::Canvas& canvas, Rect rect,
                        const std::vector<std::string>& items, std::size_t selected,
                        bool open, std::size_t active, const ThemePalette& theme,
                        const ControlState& state, const DropdownInput& input) noexcept {
    DropdownResult result;
    const bool can_open = state.enabled && !items.empty();
    const std::size_t visible_rows =
        static_cast<std::size_t>(std::max(0.0, rect.h - 1.0));
    const bool expanded = open && can_open && visible_rows >= 1;
    const std::size_t window_first =
        expanded ? dropdown_window_first(items.size(), visible_rows, active) : 0;

    // ── 意图 ──────────────────────────────────────────────────────────────
    if (state.enabled) {
        if (expanded) {
            if (input.move_delta > 0) {
                result.intent = DropdownIntent::MoveDown;
                result.index = clamp_index(active + 1, items.size());
                return result;
            }
            if (input.move_delta < 0) {
                result.intent = DropdownIntent::MoveUp;
                result.index = active == 0 ? 0 : active - 1;
                return result;
            }
            if (input.commit) {
                result.intent = DropdownIntent::Commit;
                result.index = clamp_index(active, items.size());
                return result;
            }
            if (input.cancel) {
                result.intent = DropdownIntent::Cancel;
                return result;
            }
            if (input.has_click && state.hovered) {
                const std::size_t visible_row =
                    dropdown_row_at(rect, items.size(),
                                    static_cast<std::uint32_t>(input.click_y),
                                    static_cast<std::uint32_t>(input.click_x));
                if (visible_row != kNoSelection) {
                    result.intent = DropdownIntent::Commit;
                    // 可视行 → 绝对下标：加窗口首项偏移（与绘制同口径）。
                    result.index =
                        clamp_index(window_first + visible_row, items.size());
                    return result;
                }
                // 点到折叠行/清单外：收起。
                result.intent = DropdownIntent::Toggle;
                return result;
            }
        } else if (state.hovered && state.pressed) {
            result.intent = DropdownIntent::Toggle;
        }
    }

    // ── 绘制（意图与几何正交：退化几何只跳过绘制）──────────────────────────
    if (items.empty() || rect.empty()) {
        return result;
    }
    const Rgb fg = state.enabled ? theme.foreground : theme.text_dim;
    Style base = style_fg_bg(fg, theme.background);
    if (state.focused) {
        base.set_attr(Attr::Bold);  // 聚焦的折叠行加粗（与 checkbox 一致）
    }
    const Style bracket = style_fg(state.enabled ? theme.accent : theme.text_dim);

    // 折叠行：`[ 当前值 ]`
    canvas.fill_rect(Rect{rect.x, rect.y, rect.w, 1.0}, U' ', base);
    const std::string_view current = items[clamp_index(selected, items.size())];
    const double text_x = rect.x + 2.0;
    const double avail = rect.x + rect.w - 1.0 - text_x;
    if (avail >= 1.0) {
        const auto max_cp = static_cast<std::size_t>(avail);
        const std::string_view shown = current.substr(0, utf8_take(current, max_cp));
        canvas.put(Point{rect.x, rect.y}, U'[', bracket);
        canvas.put(Point{rect.x + 1.0, rect.y}, U' ', base);
        canvas.draw_text(Point{text_x, rect.y}, shown, base);
        const std::size_t shown_cp = codepoints_before(shown, shown.size());
        canvas.put(Point{text_x + static_cast<double>(shown_cp), rect.y}, U']', bracket);
    }

    if (!expanded) {
        return result;
    }

    // 展开清单：第 1..visible_rows 行。开窗保证 active 可见。
    const std::size_t shown = std::min(items.size(), visible_rows);
    const std::size_t first = window_first;
    for (std::size_t r = 0; r < shown; ++r) {
        const std::size_t idx = first + r;
        const bool is_active = idx == active;
        const bool is_selected = idx == selected;
        const Style row_style =
            is_active ? style_fg_bg(theme.background, theme.accent) : base;
        const double y = rect.y + static_cast<double>(r + 1);
        canvas.fill_rect(Rect{rect.x, y, rect.w, 1.0}, U' ', row_style);
        if (is_selected) {
            canvas.put(Point{rect.x, y}, U'\u2022', style_fg(theme.accent));  // "•"
            canvas.put(Point{rect.x + 1.0, y}, U' ', row_style);
        }
        const double label_x = rect.x + 2.0;
        const double label_avail = rect.x + rect.w - label_x;
        if (label_avail >= 1.0) {
            canvas.draw_text(
                Point{label_x, y},
                items[idx].substr(0, utf8_take(items[idx], static_cast<std::size_t>(label_avail))),
                row_style);
        }
    }
    return result;
}

std::size_t dropdown_row_at(Rect rect, std::size_t item_count, std::uint32_t row,
                            std::uint32_t col) noexcept {
    if (item_count == 0 || rect.empty()) {
        return kNoSelection;
    }
    // 行 0 是折叠行，候选项从第 1 行起；col 须落在 rect 内。
    if (row < 1) {
        return kNoSelection;
    }
    const auto in_col = static_cast<std::uint32_t>(rect.x) <= col &&
                        col < static_cast<std::uint32_t>(rect.x + rect.w);
    if (!in_col) {
        return kNoSelection;
    }
    const auto row_index = static_cast<std::size_t>(row - 1);
    const auto max_rows = static_cast<std::size_t>(std::max(0.0, rect.h - 1.0));
    if (row_index >= max_rows) {
        return kNoSelection;
    }
    // 返回**可视行内序号（0 起）**：候选项多于可视行时它是相对窗口首项的偏移，
    // 绝对下标由 dropdown() 内部加 window_first 得到（两边同一口径）。
    return row_index < item_count ? row_index : kNoSelection;
}

// ── 文本框 ────────────────────────────────────────────────────────────────

TextBoxResult textbox(canvas::Canvas& canvas, Rect rect, std::string& text,
                      const ThemePalette& theme, const ControlState& state,
                      const TextBoxInput& input) noexcept {
    TextBoxResult result;
    std::size_t caret = std::min(input.caret, text.size());

    if (state.enabled) {
        // ── 编辑 ──────────────────────────────────────────────────────────
        std::size_t i = caret;
        for (int step = 0; step < input.move_delta; ++step) {
            i = utf8_next(text, i);
        }
        for (int step = 0; step < -input.move_delta; ++step) {
            i = utf8_prev(text, i);
        }
        for (int step = 0; step < input.erase_back; ++step) {
            if (i == 0) {
                break;
            }
            const std::size_t prev = utf8_prev(text, i);
            text.erase(prev, i - prev);
            i = prev;
            result.changed = true;
        }
        for (int step = 0; step < input.erase_forward; ++step) {
            if (i >= text.size()) {
                break;
            }
            const std::size_t next = utf8_next(text, i);
            text.erase(i, next - i);
            result.changed = true;
        }
        for (const char ch : input.inserted) {
            if (static_cast<unsigned char>(ch) < 0x20u) {
                continue;  // 控制字符不进文本框
            }
            text.insert(i, 1, ch);
            ++i;
            result.changed = true;
        }
        result.submitted = input.submit;
        caret = i;
    }
    result.caret = caret;

    // ── 绘制 ──────────────────────────────────────────────────────────────
    if (rect.w < 3.0 || rect.h < 1.0) {
        return result;
    }
    const Rgb fg = state.enabled ? theme.foreground : theme.text_dim;
    const Style base =
        state.hovered ? style_fg_bg(fg, theme.highlight_bg) : style_fg_bg(fg, theme.background);
    const Style bracket = style_fg(state.focused ? theme.accent : theme.border);
    canvas.fill_rect(Rect{rect.x, rect.y, rect.w, 1.0}, U' ', base);
    canvas.put(Point{rect.x, rect.y}, U'[', bracket);
    canvas.put(Point{rect.x + rect.w - 1.0, rect.y}, U']', bracket);

    const double text_x = rect.x + 2.0;
    const double avail = rect.x + rect.w - 2.0 - text_x;  // 两侧括号各让 1 格
    if (avail < 1.0) {
        return result;
    }
    const auto max_cp = static_cast<std::size_t>(avail) - 1;  // 末尾留给光标
    if (max_cp == 0) {
        return result;
    }
    const std::size_t caret_cp = codepoints_before(text, caret);
    std::size_t start_cp = caret_cp + 1 > max_cp ? caret_cp + 1 - max_cp : 0;
    // 从 start_cp 起逐码点推进到字节偏移。
    std::size_t start_byte = 0;
    for (std::size_t c = 0; c < start_cp; ++c) {
        start_byte = utf8_next(text, start_byte);
    }
    const std::string_view window = text.substr(start_byte, utf8_take(text.substr(start_byte), max_cp));
    canvas.draw_text(Point{text_x, rect.y}, window, base);
    const std::size_t caret_col = caret_cp - start_cp;
    const bool caret_visible = caret_col <= max_cp;
    if (caret_visible) {
        Style caret_style = style_fg_bg(fg, theme.background);
        caret_style.set_attr(Attr::Reverse);
        canvas.put(Point{text_x + static_cast<double>(caret_col), rect.y}, U' ', caret_style);
    }
    return result;
}

// ── 单选组 ────────────────────────────────────────────────────────────────

std::size_t radio_group_clicked(canvas::Canvas& canvas, Rect rect,
                                const std::vector<std::string>& items, std::size_t selected,
                                std::size_t hovered_row, bool pressed, bool enabled, bool focused,
                                const ThemePalette& theme) noexcept {
    const bool clicked = enabled && pressed && hovered_row != kNoSelection;

    if (rect.empty() || items.empty()) {
        return clicked ? hovered_row : kNoSelection;
    }

    const Rgb fg = enabled ? theme.foreground : theme.text_dim;
    const Style base = style_fg_bg(fg, theme.background);
    const Style mark_on = style_fg(enabled ? theme.accent : theme.text_dim);
    const Style mark_off = style_fg(enabled ? theme.border : theme.text_dim);

    for (std::size_t i = 0; i < items.size(); ++i) {
        const double y = rect.y + static_cast<double>(i);
        if (y >= rect.y + rect.h) {
            break;  // 行超出 rect：不绘制
        }
        const bool is_selected = i == selected;
        const bool is_hovered = i == hovered_row;
        Style row = is_hovered ? style_fg_bg(fg, theme.highlight_bg) : base;
        if (is_selected && focused) {
            row.set_attr(Attr::Bold);
        }
        canvas.fill_rect(Rect{rect.x, y, rect.w, 1.0}, U' ', row);
        canvas.put(Point{rect.x, y}, U'(', is_selected ? mark_on : mark_off);
        canvas.put(Point{rect.x + 1.0, y}, is_selected ? U'\u2022' : U' ',
                   is_selected ? mark_on : mark_off);
        canvas.put(Point{rect.x + 2.0, y}, U')', is_selected ? mark_on : mark_off);
        const double label_x = rect.x + 4.0;
        const double avail = rect.x + rect.w - label_x;
        if (avail >= 1.0) {
            canvas.draw_text(
                Point{label_x, y},
                items[i].substr(0, utf8_take(items[i], static_cast<std::size_t>(avail))), row);
        }
    }
    return clicked ? hovered_row : kNoSelection;
}

std::size_t radio_row_at(Rect rect, std::size_t item_count, Point p) noexcept {
    if (item_count == 0 || rect.empty() || p.x < rect.x ||
        p.x >= rect.x + rect.w) {
        return kNoSelection;
    }
    if (p.y < rect.y || p.y >= rect.y + rect.h) {
        return kNoSelection;
    }
    const auto row = static_cast<std::size_t>(p.y - rect.y);
    return row < item_count ? row : kNoSelection;
}

}  // namespace termchart::widget
