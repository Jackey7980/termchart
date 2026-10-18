// controls.hpp —— 无状态即时组件：下拉框 / 单行文本框 / 单选组
//
// 模型与 immediate_widget.hpp（按钮/复选框）完全一致：
//   * **不持有状态**：开合、高亮项、光标、选中项全部由调用方持有，每帧传入；
//   * **返回"意图"不返回"沿"**：hovered+pressed 即报告点击/提交；调用方比对
//     自己上一帧收到的返回值即可得到沿；
//   * **事件与几何正交**：退化几何只跳过绘制，不影响返回值；
//   * 文本一律按 UTF-8 码点操作（光标按码点移动、退格删整个码点），
//     宽度按主码点数估（宽字符按 1 格计）。
#ifndef TERMCHART_WIDGET_CONTROLS_HPP
#define TERMCHART_WIDGET_CONTROLS_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/widget/immediate_widget.hpp"

namespace termchart::widget {

// “无选中 / 无命中”哨兵：std::size_t 没有负值，用全 1 表示（调用方判 != kNoSelection）。
constexpr std::size_t kNoSelection = static_cast<std::size_t>(-1);

// ── 下拉框 ────────────────────────────────────────────────────────────────

enum class DropdownIntent : std::uint8_t {
    None,
    Toggle,    // 折叠↔展开（点到折叠行）
    MoveUp,    // 展开态高亮上移一项
    MoveDown,  // 展开态高亮下移一项
    Commit,    // 展开态确认（回车 / 点到某行）
    Cancel,    // 展开态取消（Esc）
};

// 本轮输入。has_click + click 单元格坐标（格坐标、向下取整）由调用方从
// 自己的事件流转来；键盘类输入用 move_delta / commit / cancel。
struct DropdownInput {
    bool has_click = false;
    std::size_t click_x = 0;
    std::size_t click_y = 0;
    int move_delta = 0;   // 高亮移动量：正下一项、负上一项（键盘或滚轮）
    bool commit = false;  // 回车
    bool cancel = false;  // Esc
};

struct DropdownResult {
    DropdownIntent intent = DropdownIntent::None;
    std::size_t index = 0;  // MoveUp/MoveDown：夹取后的新高亮；Commit：被确认项
};

// 绘制 + 意图。展开态从第 1 行起列候选项（第 0 行始终是折叠行），高亮行
// accent 反白、选中项前缀 "• "；候选项超过可视行数时开窗保证高亮可见。
// 折叠行画 "[ 当前值 ]"（括号 accent，选中值前景色）。
[[nodiscard]] TERMCHART_API DropdownResult dropdown(
    canvas::Canvas& canvas, Rect rect, const std::vector<std::string>& items,
    std::size_t selected, bool open, std::size_t active, const ThemePalette& theme,
    const ControlState& state, const DropdownInput& input = {}) noexcept;

// 展开态命中查询：输入格坐标（row/col 向下取整），返回**可视行内序号
// （0 起）**；行 0（折叠行）、清单外、已折叠（请调用方先看 open）返回
// kNoSelection。候选项多于可视行时该值是相对窗口首项的偏移——绝对下标由
// dropdown() 内部统一加窗口偏移，调用方做 hover 粗判可直接用本值。
[[nodiscard]] TERMCHART_API std::size_t dropdown_row_at(Rect rect,
                                                        std::size_t item_count,
                                                        std::uint32_t row,
                                                        std::uint32_t col) noexcept;

// ── 单行文本框 ────────────────────────────────────────────────────────────

struct TextBoxInput {
    std::size_t caret = 0;       // 光标字节偏移（调用方持有并逐帧回传）
    int move_delta = 0;          // 光标按码点移动（±1）
    int erase_back = 0;          // 退格次数（删光标前一个码点）
    int erase_forward = 0;       // Delete 次数（删光标处一个码点）
    std::string_view inserted;   // 本轮插入文本（控制字符被丢弃）
    bool submit = false;         // 回车
};

struct TextBoxResult {
    bool changed = false;     // 文本被改动
    bool submitted = false;   // 本轮回车
    std::size_t caret = 0;    // 编辑后的光标字节偏移（调用方下一帧回传）
};

// 编辑 text（原地修改）并绘制 "[ text ]" 单行。文本超宽时开窗滚动保证光标
// 可见，光标格画 Reverse 空格。disabled 时仍绘制，但不接受输入。
[[nodiscard]] TERMCHART_API TextBoxResult textbox(canvas::Canvas& canvas, Rect rect,
                                                  std::string& text,
                                                  const ThemePalette& theme,
                                                  const ControlState& state,
                                                  const TextBoxInput& input = {}) noexcept;

// ── 单选组 ────────────────────────────────────────────────────────────────

// 互斥项组：第 i 项画第 i 行（行高 1），选中项 " (•) label"、其余 " ( ) label"。
// hovered_row 由调用方用 radio_row_at 推导；pressed/enabled 语义同按钮。
// 返回本轮被点击项下标；无点击返回 kNoSelection。
[[nodiscard]] TERMCHART_API std::size_t radio_group_clicked(
    canvas::Canvas& canvas, Rect rect, const std::vector<std::string>& items,
    std::size_t selected, std::size_t hovered_row, bool pressed, bool enabled,
    bool focused, const ThemePalette& theme) noexcept;

// 命中行查询：p 落在第 i 项行内返回 i；越界/负坐标返回 kNoSelection。
[[nodiscard]] TERMCHART_API std::size_t radio_row_at(Rect rect, std::size_t item_count,
                                                    Point p) noexcept;

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_CONTROLS_HPP
