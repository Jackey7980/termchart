// clipboard.hpp —— OSC 52 剪贴板
//
// 写入： ESC ] 52 ; <target> ; <base64 文本> ST
// 读取： 同样发一条，但 base64 为空，终端会**回包**同样形状的序列
//
// 安全提醒：OSC 52 是否生效由终端决定——很多终端默认关闭写入，也有终端只允许
// 读取。因此本模块只生成序列，不假设它一定成功；调用方不应把剪贴板当作可靠存储。
#ifndef TERMCHART_TERM_CLIPBOARD_HPP
#define TERMCHART_TERM_CLIPBOARD_HPP

#include <string>
#include <string_view>

#include "termchart/export.hpp"

namespace termchart::term {

enum class ClipboardTarget : char {
    Selection = 'c',  // 系统剪贴板
    Primary = 'p',    // X11 主选择区
};

// 写入剪贴板（文本先按 UTF-8 原样 base64）。
[[nodiscard]] TERMCHART_API std::string clipboard_set(
    std::string_view text, ClipboardTarget target = ClipboardTarget::Selection);

// 请求终端回传剪贴板内容；回包交给 parse_clipboard_reply 解析。
[[nodiscard]] TERMCHART_API std::string clipboard_request(
    ClipboardTarget target = ClipboardTarget::Selection);

// 解析回包的 OSC 52 载荷（形如 "c;Zm9vYmFy"）。成功时 out 为解码后的原文。
// 空载荷（终端拒绝读取或剪贴板为空）返回 false，且不改动 out。
[[nodiscard]] TERMCHART_API bool parse_clipboard_reply(std::string_view payload,
                                                       std::string& out);

}  // namespace termchart::term

#endif  // TERMCHART_TERM_CLIPBOARD_HPP
