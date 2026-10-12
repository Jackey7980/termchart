// 11_wordcloud.cpp —— 词云（规格示例）
//
// 螺旋布局 + 权重定字号档（字符重复密度）。权重大的词先放置（占中心），
// 放不下的词被放弃——由 Wordcloud 内部实现，示例只喂数据。
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/text/wordcloud.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    chart::Wordcloud chart;
    chart.set_title("wordcloud");
    const std::vector<std::pair<std::string, double>> words{
        {"terminal", 9.0}, {"chart", 8.0}, {"utf8", 6.0}, {"braille", 5.5},
        {"sixel", 4.0}, {"ansi", 4.0}, {"grid", 3.0}, {"dsl", 3.0},
        {"graph", 2.5}, {"color", 2.0}, {"canvas", 2.0}, {"wcwidth", 1.5},
    };
    for (const auto& [text, weight] : words) {
        chart.add_word(text, weight);
    }

    canvas::Canvas canvas(56, 16);
    chart.draw({canvas});

    render::Renderer renderer;
    (void)api::write_stdout(renderer.render_full(canvas.to_buffer()));
    (void)api::write_stdout("\n");
    (void)api::flush_stdout();
    return 0;
}
