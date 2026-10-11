// input_parser.hpp —— 字节流状态机：终端输入字节 → Event
//
// 解析 ASCII、UTF-8、CSI（方向键/功能键/Home/End/PageUp…）、SS3（F1–F4）、
// SGR 鼠标（ESC [ < b ; x ; y M/m）、OSC（跳过）、Alt+字符、Ctrl+字母。
// 不完整序列留在内部缓冲，等后续字节——这是流式解析的必备性质（read() 可能
// 把一条序列切成两半）。
#ifndef TERMCHART_INTERACTION_INPUT_PARSER_HPP
#define TERMCHART_INTERACTION_INPUT_PARSER_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/export.hpp"
#include "termchart/interaction/event.hpp"

namespace termchart::interaction {

class InputParser {
public:
    // 喂入字节，返回本次能解析出的全部**完整**事件。
    TERMCHART_API std::vector<Event> feed(std::string_view bytes);

    // 把孤立的尾随 ESC 收尾成 Escape 事件（无超时机制的解析器需要显式收尾）。
    TERMCHART_API std::vector<Event> flush();

    [[nodiscard]] TERMCHART_API std::size_t pending() const noexcept;
    TERMCHART_API void reset();

private:
    std::string buffer_;
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_INPUT_PARSER_HPP
