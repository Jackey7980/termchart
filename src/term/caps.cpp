#include "termchart/term/caps.hpp"

#include <algorithm>

namespace termchart::term {
namespace {

bool contains(std::string_view haystack, std::string_view needle) noexcept {
    return haystack.find(needle) != std::string_view::npos;
}

bool is_known_256cxx_term(std::string_view term) noexcept {
    return contains(term, "256color") || contains(term, "256-colour") || contains(term, "xterm-256");
}

}  // namespace

ColorDepth color_depth_for(const CapEnvironment& env) noexcept {
    // 0) TERMCHART_COLOR 显式开关：优先级最高（"显式环境变量 > TERM 名称 > 默认"）。
    //    always/force 不改档位，只表示"非 tty 也要颜色"（由 terminal.cpp 解除兜底）。
    if (!env.color.empty()) {
        if (contains(env.color, "truecolor") || contains(env.color, "24bit")) {
            return ColorDepth::TrueColor;
        }
        if (contains(env.color, "256")) {
            return ColorDepth::Ansi256;
        }
        if (contains(env.color, "16")) {
            return ColorDepth::Ansi16;
        }
        if (contains(env.color, "none") || contains(env.color, "never") ||
            contains(env.color, "off")) {
            return ColorDepth::None;
        }
        // always / force / auto：档位仍按下面的探测走。
    }
    // 1) COLORTERM 显式声明，优先级次之。
    if (contains(env.colorterm, "truecolor") || contains(env.colorterm, "24bit")) {
        return ColorDepth::TrueColor;
    }
    // 2) Windows Terminal：WT_SESSION 是它必设、其它终端几乎不设的指纹。
    //    不用 TERM_PROGRAM —— 原生 Windows 会话里 WT 并不设它。
    if (!env.wt_session.empty()) {
        return ColorDepth::TrueColor;  // WT 渲染器完整支持 24 位色
    }
    // 3) TERM_PROGRAM 已知能力（iTerm / WezTerm / VS Code 集成终端）。
    if (contains(env.term_program, "iTerm") || contains(env.term_program, "WezTerm") ||
        contains(env.term_program, "vscode")) {
        return ColorDepth::TrueColor;
    }
    // 4) TERM 名判定。
    if (is_known_256cxx_term(env.term)) {
        return ColorDepth::Ansi256;
    }
    // 5) 只有明确"哑终端"才判无色。
    //    关键修正：删掉原来的 env.term.empty() —— TERM 为空 ≠ 不支持颜色
    //    （Windows 原生会话里 TERM 为空是常态）；真正的哑终端会设 TERM=dumb。
    if (env.term == "dumb" || contains(env.term, "mono")) {
        return ColorDepth::None;
    }
    if (contains(env.term, "color")) {
        return ColorDepth::Ansi16;
    }
    // 6) 默认：给最低能着色档位。真正的"非 tty / 管道 / 重定向"场景由
    //    daemon_safe + detect() 的 tty 参数兜底，不靠这里判空。
    return ColorDepth::Ansi16;
}

ImageProtocolKind image_protocol_for(const CapEnvironment& env) noexcept {
    if (contains(env.term_program, "iTerm")) {
        return ImageProtocolKind::ITerm2;
    }
    if (contains(env.term, "kitty") || contains(env.term_program, "kitty")) {
        return ImageProtocolKind::Kitty;
    }
    if (contains(env.term, "sixel") || contains(env.term, "xterm") || contains(env.term, "mlterm") ||
        contains(env.term, "foot")) {
        return ImageProtocolKind::Sixel;
    }
    // 半块字符永远可用：这是"总有一次降级"的底线。
    return ImageProtocolKind::Halfblock;
}

bool unicode_for(const CapEnvironment& env) noexcept {
    if (env.term == "linux" || contains(env.term, "mono") || env.term == "dumb") {
        return false;
    }
    return true;
}

TerminalCaps detect(const CapEnvironment& env, bool tty) noexcept {
    TerminalCaps caps;
    // 颜色档位是**纯函数**，不看 tty（见 term: detect 把 tty/CI 折进能力 的约定）：
    // 非 tty / 管道 / 重定向 / CI 的"无色"由 daemon_safe_for + apply 在 Terminal
    // 构造时兜底——职责分离，detect 不越权。
    caps.color_depth = color_depth_for(env);
    caps.image = image_protocol_for(env);
    caps.unicode = unicode_for(env);
    caps.mouse = tty && !env.ci;
    caps.alt_screen = tty;
    caps.title_setting = tty;
    if (env.term == "dumb") {
        caps.color_depth = ColorDepth::None;
        caps.image = ImageProtocolKind::None;
        caps.alt_screen = false;
        caps.title_setting = false;
    }
    return caps;
}

TerminalCaps minimal() noexcept {
    TerminalCaps caps;
    caps.unicode = false;
    caps.color_depth = ColorDepth::None;
    caps.image = ImageProtocolKind::None;
    caps.mouse = false;
    caps.alt_screen = false;
    caps.title_setting = false;
    return caps;
}

const char* image_protocol_name(ImageProtocolKind kind) noexcept {
    switch (kind) {
        case ImageProtocolKind::None:
            return "none";
        case ImageProtocolKind::Halfblock:
            return "halfblock";
        case ImageProtocolKind::Sixel:
            return "sixel";
        case ImageProtocolKind::Kitty:
            return "kitty";
        case ImageProtocolKind::ITerm2:
            return "iterm2";
    }
    return "unknown";
}

}  // namespace termchart::term
