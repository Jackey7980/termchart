// hyperlink.hpp —— OSC 8 超链接
//
// 序列形状（OSC 8 用 ST `ESC \` 结束，不用 BEL）：
//   打开： ESC ] 8 ; [id=<id>;]<uri> ST
//   文本
//   关闭： ESC ] 8 ; ; ST
//
// 支持的范围很广（iTerm2 / kitty / WezTerm / Windows Terminal / VTE 系），
// 不支持的终端会**原样显示** URI 与文本——因此 hyperlink() 只在明确需要时才用。
#ifndef TERMCHART_TERM_HYPERLINK_HPP
#define TERMCHART_TERM_HYPERLINK_HPP

#include <string>
#include <string_view>

#include "termchart/export.hpp"

namespace termchart::term {

// 打开超链接。id 非空时同一 id 的相邻链接会被终端视作同一个链接
// （避免鼠标悬停时闪烁），也便于"同一目标多段文本"合并。
[[nodiscard]] TERMCHART_API std::string hyperlink_open(std::string_view uri,
                                                        std::string_view id = {});

[[nodiscard]] TERMCHART_API std::string hyperlink_close();

// 便捷：一次给出"打开 + 文本 + 关闭"。
[[nodiscard]] TERMCHART_API std::string hyperlink(std::string_view text,
                                                   std::string_view uri,
                                                   std::string_view id = {});

}  // namespace termchart::term

#endif  // TERMCHART_TERM_HYPERLINK_HPP
