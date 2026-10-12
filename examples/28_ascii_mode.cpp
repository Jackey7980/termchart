// 28_ascii_mode.cpp —— 颜色降级阶梯（规格示例：降级纯文本/ASCII 场景的能力底座）
//
// 同一份内容在四种能力档位下的输出对照：
//   * TrueColor —— 38;2 真彩；
//   * Ansi256 / Ansi16 —— 量化降档（render/sgr 统一收口）；
//   * None —— SGR 全剥，**纯文本**（管道/日志安全：非 tty 时 daemon_safe 也走这档）。
// "能力降级只发生在输出边界"：数据与布局不变，变的只是序列。
#include <cstdint>
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/color_depth.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

// 用渲染管线画一行"带框标题"：块字符 + 颜色，便于对照三种档位。
std::string render_banner(ColorDepth depth) {
    canvas::Canvas canvas(24, 1);
    const Style style = Style{}.set_fg(Color::rgb(86, 197, 154)).set_attr(Attr::Bold);
    const std::string text = "termchart";
    for (std::size_t i = 0; i < text.size(); ++i) {
        canvas.put(static_cast<std::uint32_t>(i), 0, static_cast<char32_t>(text[i]), style);
    }
    render::Renderer renderer(depth);
    std::string out = renderer.render_full(canvas.to_buffer());
    out += "\n";
    return out;
}

}  // namespace

int main() {
    (void)api::write_stdout("== TrueColor ==\n");
    (void)api::write_stdout(render_banner(ColorDepth::TrueColor));
    (void)api::write_stdout("== Ansi256 ==\n");
    (void)api::write_stdout(render_banner(ColorDepth::Ansi256));
    (void)api::write_stdout("== Ansi16 ==\n");
    (void)api::write_stdout(render_banner(ColorDepth::Ansi16));
    (void)api::write_stdout("== None（纯文本）==\n");
    (void)api::write_stdout(render_banner(ColorDepth::None));
    (void)api::flush_stdout();
    return 0;
}
