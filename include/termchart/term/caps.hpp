// caps.hpp —— 终端能力探测
//
// 优先级规则见 docs/reference/terminal_capabilities.md：
//   显式环境变量 > TERM 名称 > 默认保守值。
// **纯函数**：能力探测不读写终端、不发查询序列，输入是环境变量快照 + 是否 tty。
// 因此 CI / 单元测试可以把环境变量塞进来，完全可复现。
#ifndef TERMCHART_TERM_CAPS_HPP
#define TERMCHART_TERM_CAPS_HPP

#include <string_view>

#include "termchart/core/color_depth.hpp"
#include "termchart/export.hpp"

namespace termchart::term {

// 图像协议：term 只知道"能不能画"，真正编码在 protocol/（L9）。
enum class ImageProtocolKind : std::uint8_t {
    None = 0,        // 不能内联画图：退化为半块字符
    Halfblock = 1,   // 用 ▀▄ 拼图：全平台可用
    Sixel = 2,
    Kitty = 3,
    ITerm2 = 4,
};

struct CapEnvironment {
    std::string_view term;          // TERM
    std::string_view colorterm;     // COLORTERM
    std::string_view term_program;  // TERM_PROGRAM
    bool ci = false;                // CI / GITHUB_ACTIONS 等
    // WT_SESSION：Windows Terminal 指纹。它必设、而其他终端几乎不设，是识别
    // 真彩色宿主的可靠信号（原生 Windows 会话里 TERM 往往为空）。**放在末尾**，
    // 以免破坏既有按位置初始化 {term, colorterm, term_program, ci} 的调用点。
    std::string_view wt_session;
};

struct TerminalCaps {
    bool unicode = true;
    ColorDepth color_depth = ColorDepth::TrueColor;
    ImageProtocolKind image = ImageProtocolKind::Halfblock;
    bool mouse = false;
    bool alt_screen = true;
    bool title_setting = true;
};

// 按优先级合成能力。tty == false 时先套 daemon_safe 的保守假设。
TERMCHART_API TerminalCaps detect(const CapEnvironment& env, bool tty) noexcept;

// 最保守能力：纯 ASCII、无色、无图像。用于非 tty / CI / 老旧终端。
TERMCHART_API TerminalCaps minimal() noexcept;

// 单独的规则单元，**公开以便测试**：它们是那份优先级表的可执行形式。
TERMCHART_API ColorDepth color_depth_for(const CapEnvironment& env) noexcept;
TERMCHART_API ImageProtocolKind image_protocol_for(const CapEnvironment& env) noexcept;
TERMCHART_API bool unicode_for(const CapEnvironment& env) noexcept;

TERMCHART_API const char* image_protocol_name(ImageProtocolKind kind) noexcept;

}  // namespace termchart::term

#endif  // TERMCHART_TERM_CAPS_HPP
