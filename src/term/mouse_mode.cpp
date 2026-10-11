// mouse_mode.cpp —— 鼠标上报开关实现
#include "termchart/term/mouse_mode.hpp"

#include <cstdint>
#include <string>

namespace termchart::term {
namespace {

// DECSET / DECRST：`ESC [ ? <n> h` 打开，`ESC [ ? <n> l` 关闭。
std::string decset(int number, bool enable) {
    std::string out;
    out += "\x1b[?";
    out += std::to_string(number);
    out.push_back(enable ? 'h' : 'l');
    return out;
}

int mode_number(MouseMode mode) noexcept {
    return static_cast<int>(mode);
}

int encoding_number(MouseEncoding encoding) noexcept {
    return static_cast<int>(encoding);
}

}  // namespace

std::string set_mouse_mode(MouseMode mode) noexcept {
    if (mode == MouseMode::Off) {
        return {};
    }
    return decset(mode_number(mode), true);
}

std::string set_mouse_encoding(MouseEncoding encoding) noexcept {
    if (encoding == MouseEncoding::Default) {
        return {};
    }
    return decset(encoding_number(encoding), true);
}

std::string enable_mouse(MouseMode mode, MouseEncoding encoding) {
    // 顺序：先定编码再开模式。终端解析这两条互不影响，但先编码更符合直觉
    // （万一终端只认其中一条，至少坐标含义是对的）。
    std::string out = set_mouse_encoding(encoding);
    out += set_mouse_mode(mode);
    return out;
}

std::string disable_mouse(MouseMode mode, MouseEncoding encoding) {
    std::string out;
    if (mode != MouseMode::Off) {
        out += decset(mode_number(mode), false);
    }
    if (encoding != MouseEncoding::Default) {
        out += decset(encoding_number(encoding), false);
    }
    return out;
}

std::string disable_all_mouse(MouseEncoding encoding) {
    std::string out;
    for (const MouseMode mode : {MouseMode::X10, MouseMode::Normal, MouseMode::ButtonMotion,
                                 MouseMode::AnyMotion}) {
        out += decset(mode_number(mode), false);
    }
    if (encoding != MouseEncoding::Default) {
        out += decset(encoding_number(encoding), false);
    }
    return out;
}

}  // namespace termchart::term
