// mouse_mode.hpp —— 鼠标上报开关（DECSET / DECRST）
//
// 只生成序列，不写 stdout、不改自身状态：调用方（term/terminal 或 app/）决定何时
// 打开、何时关闭，并负责在异常退出时复位（见 term/terminal 的 RAII 门面）。
//
// 模式与坐标编码是**两组独立开关**，必须分别设置：
//   * 模式（9 / 1000 / 1002 / 1003）决定"什么时候上报"；
//   * 编码（1005 / 1006 / 1015）决定"怎么编码坐标"。
// 默认编码是 X10 风格，坐标上限 223 列且有歧义，所以实际使用一律建议配 SGR(1006)。
#ifndef TERMCHART_TERM_MOUSE_MODE_HPP
#define TERMCHART_TERM_MOUSE_MODE_HPP

#include <cstdint>
#include <string>

#include "termchart/export.hpp"

namespace termchart::term {

// 枚举值**就是** DECSET 的编号本身（9 / 1000 / 1002 / 1003 与 1005 / 1006 / 1015），
// 生成序列时直接写进 `ESC [ ? <n> h`，不必再维护一张映射表。
enum class MouseMode : std::uint16_t {
    Off = 0,               // 不上报
    X10 = 9,               // 只上报按下
    Normal = 1000,         // 按下 / 释放 / 滚轮
    ButtonMotion = 1002,   // 按住拖动时上报移动
    AnyMotion = 1003,      // 任何移动都上报
};

enum class MouseEncoding : std::uint16_t {
    Default = 0,    // 终端默认（X10 风格，坐标上限 223）
    Utf8 = 1005,    // UTF-8 扩展坐标
    Sgr = 1006,     // SGR 扩展：坐标无上限，且能明确区分按下与释放（推荐）
    Urxvt = 1015,   // urxvt 扩展
};

// 打开/关闭指定模式；mode == Off 时本函数不产生模式序列。
[[nodiscard]] TERMCHART_API std::string set_mouse_mode(MouseMode mode) noexcept;

// 打开/关闭指定坐标编码；encoding == Default 时返回空串。
[[nodiscard]] TERMCHART_API std::string set_mouse_encoding(MouseEncoding encoding) noexcept;

// 一次打开"模式 + 编码"（编码为 Default 时只设模式）。
[[nodiscard]] TERMCHART_API std::string enable_mouse(
    MouseMode mode, MouseEncoding encoding = MouseEncoding::Sgr);

// 关闭指定模式与编码（顺序与打开相反）。
[[nodiscard]] TERMCHART_API std::string disable_mouse(
    MouseMode mode, MouseEncoding encoding = MouseEncoding::Sgr);

// 把 9/1000/1002/1003 全部关掉：切换模式前用它清场，避免多个模式叠加
// （终端的鼠标模式是**叠加**的，只关当前模式会留下上一次打开的另一个模式）。
[[nodiscard]] TERMCHART_API std::string disable_all_mouse(
    MouseEncoding encoding = MouseEncoding::Sgr);

}  // namespace termchart::term

#endif  // TERMCHART_TERM_MOUSE_MODE_HPP
