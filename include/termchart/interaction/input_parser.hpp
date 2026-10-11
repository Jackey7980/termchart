// input_parser.hpp —— 字节流状态机：终端输入字节 → Event
//
// 解析 ASCII、UTF-8、CSI（方向键/功能键/Home/End/PageUp…）、SS3（F1–F4）、
// SGR 鼠标（ESC [ < b ; x ; y M/m）、OSC（跳过）、Alt+字符、Ctrl+字母。
// 不完整序列留在内部缓冲，等后续字节——这是流式解析的必备性质（read() 可能
// 把一条序列切成两半）。
//
// 但缓冲是**有上限的**（`kMaxPendingBytes`）：积压超过上限说明收到的不是"还没收完
// 的一条序列"，而是垃圾或恶意输入（例如 `ESC ]` 之后跟着无穷多个字节，永远等不到
// OSC 终止符）。此时**丢弃缓冲并跳到下一个 ESC 重新同步**，避免内存无界增长，也避免
// 把被丢弃序列的中段当成按键产出伪事件。
//
// 代价要说清楚：超过上限的**合法**长序列也会被丢弃（例如 iTerm2 内联图像的 base64
// 负载本身就可能大于 256 KiB）。本库的解析器本来就不产出 OSC 内容事件，所以常规用法
// 不受影响；若你的应用需要接收这种超长 OSC，请在应用层单独处理，或调大这个常量。
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
    // 未完成序列的缓冲上限（256 KiB）。超限即丢弃缓冲重新同步，见文件头说明。
    static constexpr std::size_t kMaxPendingBytes = 256 * 1024;

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
