// 01_hello_color.cpp —— 终端上色并正常退出
//
// 阶段 1 的第一个出口条件：证明"上色 + 恢复"这条最短链路成立。
// 演示的是**显式**用法：自己搭 CellBuffer、自己选样式、自己交给 Renderer。
// 零配置版本见 32_quick_one_liner.cpp。
#include <cstdio>
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/term/caps.hpp"
#include "termchart/term/terminal.hpp"
#include "termchart/color/rgba.hpp"

namespace {

termchart::CellBuffer build_buffer(termchart::ColorDepth depth) {
    termchart::CellBuffer buffer(24, 2);

    termchart::Style banner;
    banner.set_fg(termchart::Color::rgb(86, 197, 154)).set_attr(termchart::Attr::Bold);
    const std::string first = "termchart v0.1";
    for (std::size_t i = 0; i < first.size(); ++i) {
        buffer.at(static_cast<std::uint32_t>(i), 0) =
            termchart::Cell{static_cast<char32_t>(first[i]), banner};
    }

    termchart::Style hint;
    // 终端 SGR 无 alpha 通道：半透明前景须先压平到背景色再上屏。
    // RGBA(182,143,190,128) 与背景混合 → 不透明前景（blend 在 color 层）。
    const termchart::Rgb bg = termchart::Rgb{30, 30, 36};
    const termchart::Rgb flat =
        termchart::color::blend(termchart::color::Rgba{225, 72, 0, 1}, bg); // rgba(255, 72, 0, 1) 半透明前景
    hint.set_bg(termchart::Color::rgb(flat)).set_fg(termchart::Color::rgb(flat));
    const std::string second = "hello, colored terminal";
    for (std::size_t i = 0; i < second.size(); ++i) {
        buffer.at(static_cast<std::uint32_t>(i), 1) =
            termchart::Cell{static_cast<char32_t>(second[i]), hint};
    }

    // 深度不足时由 render/sgr.hpp 统一降级，示例自身不做任何 SGR 拼接。
    (void)depth;
    return buffer;
}

}  // namespace

int main() {
    // RAII：异常退出（含 return 之外的路径）也会恢复终端。
    termchart::term::Terminal terminal(termchart::term::TerminalOptions{
        .alt_screen = false, .hide_cursor = false});
    const termchart::Status entered = terminal.enter();
    if (entered != termchart::ErrorCode::Ok) {
        std::fprintf(stderr, "无法进入终端模式: %s\n", termchart::describe(entered));
        return 1;
    }

    const termchart::CellBuffer buffer = build_buffer(terminal.caps().color_depth);
    termchart::render::Renderer renderer(terminal.caps().color_depth);
    const std::string bytes = renderer.render(buffer);

    if (const termchart::Status written = termchart::api::write_stdout(bytes);
        written != termchart::ErrorCode::Ok) {
        std::fprintf(stderr, "写出失败: %s\n", termchart::describe(written));
        return 1;
    }
    const termchart::Status newline = termchart::api::write_stdout("\n");
    if (newline != termchart::ErrorCode::Ok) {
        return 1;
    }

    (void)termchart::api::flush_stdout();
    return terminal.leave() == termchart::ErrorCode::Ok ? 0 : 1;
}
